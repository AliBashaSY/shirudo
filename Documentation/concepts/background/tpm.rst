..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: Explanation
:audience: All readers

TPM 2.0 and how shirudo uses it
===============================

A target's TPM 2.0 does two jobs for shirudo. It holds a per-boot
measurement register that records every change to the target's BPF
state, and it signs evidence about that register and the boot PCRs, so
that an operator can check a target without taking its word for
anything. This page explains the parts of TPM 2.0 those jobs rely on
and, for each, what shirudo does with it. The event formats and the
eventlog are in :doc:`/internals/measurement-register`, the
verification procedure in :doc:`/operate/attest`.

Every target needs a TPM 2.0 with an active SHA-256 PCR bank. There is
no unmeasured mode. The operator host never talks to a TPM: it
computes and checks everything in software. Table 1 lists the TPM
objects shirudo uses.

.. list-table:: Table 1. TPM objects shirudo uses
   :header-rows: 1
   :widths: 22 30 48

   * - Object
     - Kind
     - Purpose
   * - Measurement register
     - NV extend index ``0x01818E00``, owner hierarchy
     - Records ``init``, upatch loads and unloads, tree operations and
       key revocations. Clears at every TPM reset and TPM restart.
   * - NV writer key
     - RSA-3072 primary key, owner hierarchy
     - Authorizes each extension of the register. A new key every
       host boot.
   * - Attestation key
     - ECC NIST P-256 primary key, endorsement hierarchy
     - Signs quotes and NV certifications. The same key for the life
       of the TPM's endorsement seed.
   * - Endorsement key
     - RSA 2048 or ECC NIST P-256 primary key, endorsement hierarchy
     - Ties the attestation key to a certified TPM through credential
       activation.
   * - PCR 9
     - SHA-256 bank
     - Carries the register's anchor.
   * - Config PCR
     - SHA-256 bank, chosen by ``config.pcr``
     - Records config image activations, see
       :doc:`/internals/config-images`.

Roots of trust
--------------

A TPM is a small security processor separate from the main CPU: a
discrete chip, a firmware TPM in the processor's secure environment,
or a virtual TPM that a hypervisor or ``swtpm`` provides to a virtual
machine. It keeps keys and registers in shielded memory that only its
own commands can reach, and it never releases a private key it
generated.

The TCG specification names three roots of trust that a trusted
platform needs. The root of trust for measurement is the first code
that runs after reset; it measures the next stage and starts the chain
that :doc:`boot` describes. The TPM is the root of trust for storage,
because it keeps measurements and keys out of reach of software, and
the root of trust for reporting, because it signs statements about
what it holds. shirudo trusts the TPM in both roles. The software path
between shirudo and the TPM is a different matter, which the limits at
the end of this page cover.

Hierarchies and seeds
---------------------

TPM 2.0 groups its keys into hierarchies. Each hierarchy has a secret
seed that never leaves the TPM and its own authorization:

- The platform hierarchy belongs to the firmware. Its seed is the
  platform primary seed, and its authorization is not available to the
  operating system.
- The storage, or owner, hierarchy belongs to the machine's owner. Its
  seed is the storage primary seed, its authorization the owner
  authorization. ``TPM2_Clear`` replaces the seed, which invalidates
  every key derived from it.
- The endorsement hierarchy carries the TPM's identity. Its seed, the
  endorsement primary seed, is created at manufacturing and changes
  only through ``TPM2_ChangeEPS`` under platform authorization. Its own
  authorization is the endorsement authorization.
- The null hierarchy has a seed that the TPM replaces at every reset.
  Its keys last until the next reset.

shirudo uses the owner hierarchy for the measurement register and the
NV writer key, and the endorsement hierarchy for the attestation key
and the endorsement key. It never uses the platform or null
hierarchies. When the TPM has an owner authorization, shirudo takes it
from ``tpm.owner_auth`` in its authenticated configuration. The
endorsement authorization must be empty: shirudo has no setting for
it.

Primary keys from templates
---------------------------

A TPM can hold only a few keys at a time, so TPM 2.0 lets software
derive keys again instead of storing them. ``TPM2_CreatePrimary``
takes a hierarchy and a template, the public area of the key: its
algorithm and size, attributes, policy, and a ``unique`` field the
caller may fill with any bytes. The TPM derives the key from the
hierarchy's seed and the template. The same seed and the same template
give the same key every time, so the key needs no persistent slot.
Software creates it when it needs it and flushes it afterwards.

A key's Name is its name algorithm followed by the hash of its public
area. The Name commits to the public key and to every attribute, so
two parties that agree on a Name agree on the key and on what the TPM
allows it to do.

shirudo keeps no persistent objects in the TPM. Each command creates
the keys it needs and flushes them before it exits. The attestation
key's template is fixed, so its Name stays the same as long as the
endorsement seed does, and an operator can pin it once at enrollment.
The NV writer key's ``unique`` field is 32 random bytes that ``init``
draws at every host boot, so every boot has its own writer key.
``init`` freezes those bytes and the resulting public modulus in
shirudo's identity map, a BPF map nothing can change for the rest of
the boot. Every later command creates the key again from the frozen
bytes and refuses one whose modulus differs. A ``TPM2_Clear`` replaces
the owner seed, so the key no longer matches and extensions fail
closed.

Endorsement key and certificate
-------------------------------

The endorsement key (EK) is the TPM's identity. It is a primary key in
the endorsement hierarchy created from a template that the TCG
defines, so anyone who knows the template knows which key a given TPM
derives. The manufacturer creates the key at production, signs a
certificate for it and stores the certificate in the TPM's NV memory:
the RSA 2048 certificate at NV index ``0x01C00002``, the ECC NIST
P-256 one at ``0x01C0000A``. A verifier that trusts the manufacturer's
certificate authority can tell that an EK belongs to a genuine TPM.

The EK is a restricted decryption key, and its policy,
``TPM2_PolicySecret`` on the endorsement hierarchy, requires the
endorsement authorization to use it. It cannot sign. It decrypts
secrets that were encrypted to it, which is how credential activation,
described below, proves where another key lives.

``shirudo attest`` reads the EK certificate, the ECC one first, and
creates the EK of the certificate's type from the TCG default
template, or the ECC EK when the TPM holds no certificate. The evidence
carries the EK's public area and the certificate. ``shirudo attest
challenge`` checks that the public area is the TCG default template
under the endorsement policy and that the certificate describes that
key. With ``--ek-ca`` it also checks that the certificate chains to
the issuers given. Otherwise the operator pins the EK with
``--ek-name``, or accepts it unvouched with ``--trust-presented-ek``.

Attestation keys
----------------

A key that signs whatever a caller asks would make a quote worthless,
because software could have it sign a quote it made up. A restricted
signing key avoids this. The TPM signs with it only structures the TPM
built itself, such as quotes and certifications, which start with the
value ``TPM_GENERATED_VALUE`` (``0xFF544347``), and it refuses to sign
outside data that could pass for one. A structure that starts with
that value and carries a valid signature by a restricted key came from
the TPM that holds the key.

shirudo's attestation key (AK) is an ECC NIST P-256 primary key in the
endorsement hierarchy that signs with ECDSA and SHA-256. Its attributes
are ``fixedTPM``, ``fixedParent``, ``sensitiveDataOrigin``,
``userWithAuth``, ``restricted`` and ``sign``, and it has no
authorization value and no policy. ``init`` records its Name in the
identity map. Before each write to the register, shirudo creates the
AK again and compares its Name with the recorded one, which detects a
different TPM behind the device path. ``attest verify`` checks that
the AK's public area has these attributes, that the Name matches the
public area and, with ``--ak-name``, that it is the key pinned at
enrollment.

PCRs, banks and extension
-------------------------

A PCR holds a digest that software can only extend: the TPM replaces
the value with the hash of the old value and the new digest. A TPM
keeps one bank of PCRs per hash algorithm, typically SHA-1 and
SHA-256, and an extension names a digest for each bank it changes. A
PC Client TPM has 24 PCRs per bank. A TPM reset, or a TPM restart
after hibernation, returns them to their initial values. Apart from a
few PCRs that are resettable on purpose, such as PCR 23 for
applications, nothing else does. Any process that can send commands
to the TPM can read the PCRs and extend the ones the operating system
uses. :doc:`boot` lists who extends which PCR during a boot.

shirudo works in the SHA-256 bank only, and ``init`` refuses a TPM
whose SHA-256 bank lacks PCR 9 or PCR 11. ``init`` reads PCRs 9 and 11
when it binds the register and extends PCR 9 with the register's
anchor. Config image activation extends the config PCR.
``shirudo attest`` quotes PCRs 0 to 7, 9 and 11 to 15 by default, and
the config PCR as well when a config image is active.

NV indices and the extend type
------------------------------

Besides PCRs, a TPM has NV indices: storage locations that an owner
defines with ``TPM2_NV_DefineSpace``. Each has a handle, a size,
attributes, an authorization value and a policy. Their types include
ordinary data, counters, bit fields and extend indices. An extend
index behaves like a PCR: ``TPM2_NV_Extend`` sets it to the hash of
the old value and the new data. The first extension after the index
is defined starts from a zero digest and sets the attribute
``TPMA_NV_WRITTEN``.

The attributes decide who may read and write an index and when it
clears. With ``TPMA_NV_ORDERLY`` the TPM keeps the value in RAM and
saves it only at an orderly shutdown. Together with
``TPMA_NV_CLEAR_STCLEAR`` the index loses its value and
``TPMA_NV_WRITTEN`` at every TPM reset and TPM restart, so it starts
over with each boot, like a PCR.

An index's Name is the hash of its public area: handle, name
algorithm, attributes, policy and size. ``TPMA_NV_WRITTEN`` is one of
the attributes, so the Name changes at the first write. Unlike a PCR,
an index can be deleted with ``TPM2_NV_UndefineSpace`` and defined
again by anyone who satisfies the owner authorization. A recreated
index starts empty. This is why systemd and shirudo record an
index's Name in a PCR, which cannot be deleted.

shirudo's register is the index ``0x01818E00``, which a build can
change with ``SHIRUDO_NV_INDEX``. It holds one SHA-256 digest and has
the attributes ``0x0E060048``: ``TPM2_NT_EXTEND``,
``TPMA_NV_POLICYWRITE``, ``TPMA_NV_OWNERREAD``, ``TPMA_NV_AUTHREAD``,
``TPMA_NV_NO_DA``, ``TPMA_NV_ORDERLY`` and ``TPMA_NV_CLEAR_STCLEAR``.
Its authorization value is empty, so anyone can read it, and only a
policy session can write it. It is an NV index rather than a PCR
because a typical Linux system has no PCR left over, and PCR 23 can be
reset by any application.

Authorization and sessions
--------------------------

Every command that uses a protected entity carries an authorization,
in one of three forms.

A password authorization carries the entity's authorization value in
the command, in plaintext. Anything that sees the command sees the
password.

An HMAC session proves knowledge of the authorization value without
sending it: the caller computes an HMAC over the command and over
nonces from both sides, and the TPM answers with an HMAC over its
response. A session is salted when the caller encrypts a random salt to
a TPM key it trusts. Only that TPM can recover the salt, so only it can
compute the session key and the HMACs on its responses. A session is
bound when it mixes in the authorization value of a chosen entity. A
session that is neither has no session key. Sessions can also encrypt
the first parameter of commands and responses.

A policy session authorizes by conditions instead of a secret. The
caller runs policy assertions on the session, and each one that holds
extends the session's policy digest. The TPM allows the command only
when the final digest equals the entity's policy. The assertions
shirudo uses are:

- ``TPM2_PolicyPCR``: the selected PCRs hold the values whose hash the
  policy contains.
- ``TPM2_PolicyNvWritten``: the index has, or has not, been written.
- ``TPM2_PolicyOR``: the digest so far equals one of the listed branch
  digests.
- ``TPM2_PolicySigned``: a key with a given Name signed the hash of
  the session's TPM nonce, an expiration, a command parameter hash
  (cpHash) and a policy reference. The nonce ties the signature to this
  session, and the cpHash restricts it to one command with exactly
  these parameters.
- ``TPM2_PolicyCommandCode``: the session may authorize only this
  command.
- ``TPM2_PolicySecret``: the caller satisfies another entity's
  authorization. The EK's policy uses it.

``TPM2_PolicyGetDigest`` returns the session's digest so far, which
lets a caller compare it with the digest it computed.

shirudo uses password authorizations and policy sessions, and no HMAC
sessions. Its policy sessions are unsalted and unbound and encrypt no
parameters. The owner authorization, when set, travels as a password.

Register binding
----------------

The register has to record what shirudo did and nothing else. Root on
the target must not be able to add events of its own, delete the
register and start a fresh history, or bind the register later from a
state it chose. An NV index alone stops none of this: root can define
an index, write it and delete it. We combine the policy assertions
above, the boot phases that :doc:`boot` describes and an LSM hook.

Figure 1 shows how ``init`` binds the register from the initrd.

.. figure:: /images/background-tpm-register.svg
   :alt: PCR 9 and PCR 11 values enter the write policy, the policy
         defines the NV index, and the index's written Name is
         extended into PCR 9 as the anchor.

   Figure 1. Binding the register: (1) read PCRs 9 and 11, (2) define
   the index with the write policy, (3) write the first event through
   the init branch, (4) extend PCR 9 with the anchor.

1. ``init`` reads PCRs 9 and 11. It computes the write policy:
   ``TPM2_PolicyOR`` of a written branch, ``TPM2_PolicyNvWritten(YES)``,
   and an init branch, ``TPM2_PolicyNvWritten(NO)`` followed by
   ``TPM2_PolicyPCR`` over PCRs 9 and 11 at the values it read; then
   ``TPM2_PolicySigned`` by the NV writer key with the policy
   reference ``shirudo-nv-writer-v1``; then
   ``TPM2_PolicyCommandCode`` for ``TPM2_NV_Extend``. It also computes
   the index's Names before and after the first write.
2. It defines the index with that policy, replacing any index at the
   handle that is written or has another Name, and refuses with
   ``EPROTO`` if the Name the TPM reports differs from the computed
   one.
3. It extends the index with the init event through the init branch.
   The TPM evaluates each assertion, the writer key signs the session
   nonce and the cpHash of this ``TPM2_NV_Extend``, and
   ``TPM2_PolicyGetDigest`` confirms that the session reached the
   computed policy. The index is now written and has its written Name.
4. It extends PCR 9 with the SHA-256 of
   ``nvpcr-init:shirudo:0x1818e00:<written-name>``, systemd's anchor
   format, where ``<written-name>`` is the index's written Name in
   hex.

After step 4, PCR 9 no longer holds the value the init branch
requires, and once systemd measures ``leave-initrd``, PCR 11 does not
either. Nothing can satisfy the init branch again before the next TPM
reset, so an index that is deleted and recreated under the same policy
can never be written. An index recreated under another policy has
another Name, which matches neither the anchor in PCR 9 nor the record
``init`` kept.

Every later extension goes through the written branch, which also
requires the writer key's signature over the session nonce and the
cpHash of that one command. Before it extends, shirudo checks that the
live index carries the Name and policy ``init`` recorded, that the TPM
reports the recorded reset count, and that the writer key it creates
from the frozen bytes has the recorded modulus.

The signature alone does not keep other processes out. The bytes the
writer key derives from are not a secret, so any process that recreates
the key could ask the TPM to sign. An LSM hook in the kernel keeps
them out. The companion kernel patch
``kernel/0002-tpm-Let-LSMs-inspect-userspace-commands.patch`` adds an
LSM hook, ``tpm_command``, that sees every command written to
``/dev/tpm0`` or ``/dev/tpmrm0`` before the driver sends or queues it,
whichever process holds the descriptor. shirudo's ``lsm/tpm_command``
program refuses ``TPM2_NV_Extend`` of the frozen index to any task
without runtime authority. It also refuses, to every task, a command
too short to carry a command code and a ``TPM2_NV_Extend`` too short to
name its index. Each refusal counts as ``TpmExtend``. All other
commands pass, including extensions of other NV indices, signing and
PCR extensions.

The construction follows systemd's NvPCRs, which :doc:`boot`
introduces: the same index type, the two-branch policy and the
``nvpcr-init`` anchor. It differs in three ways. systemd's written
branch needs no further authorization, so anyone can append to an
NvPCR; shirudo requires a signed, command-bound authorization and the
LSM check on every write, because its events assert that an operation
happened. systemd binds the init branch to PCR 11 through a signed PCR
policy from the UKI, or through a plain ``TPM2_PolicyPCR`` on PCR 11
where none exists; shirudo binds it with a plain ``TPM2_PolicyPCR``
over PCRs 9 and 11, so the register's Name changes with the initrd and
the UKI, and the operator pins both values with ``--expect-bind-pcr``.
And the register is not a ``.nvpcr`` definition: systemd neither
initializes nor extends it, and its anchor appears in shirudo's
eventlog, not in systemd's measurement log.

Quotes and NV certification
---------------------------

``TPM2_Quote`` signs a ``TPMS_ATTEST`` structure with a restricted
key. The structure carries ``TPM_GENERATED_VALUE``, the qualified Name
of the signing key, the caller's nonce, the TPM's clock information
and firmware version, the PCR selection and the digest of the selected
PCR values. It proves that the TPM held those values when it signed,
and the nonce proves that it signed after the verifier asked.
``TPM2_NV_Certify`` signs the same kind of structure for an NV index,
with the index's Name, an offset and the contents.

``shirudo attest`` reads the selected PCRs, quotes them with the
verifier's nonce and keeps the quote only when its digest covers the
values it read, retrying up to three times. It certifies the
register's 32 bytes at offset 0 with the same nonce. The evidence
document also carries the public areas of the AK and EK, this boot's
eventlog records, the config history, the firmware and systemd event
logs and the EK certificate.

Only the quote and the certification are signed by the TPM. The rest
of the document is the target's own account, which ``attest verify``
checks against the signed values. In particular, it recomputes the
register's Name from the PCR values and the writer key that the init
record names. A register bound under other PCR values or with another
writer key has another Name and fails. :doc:`/operate/attest` lists
the checks and the options that pin each value.

Credential activation
---------------------

A signature by an AK proves nothing about where the AK lives until its
Name is tied to a TPM the verifier trusts. The EK certificate
identifies a TPM, but the EK cannot sign. Credential activation
connects the two.

``TPM2_MakeCredential`` needs no TPM; anyone can compute it in software
from the EK's public key. It encrypts a secret so that only the TPM
holding that EK can recover it, and only for a key with a given Name.
``TPM2_ActivateCredential`` takes the EK and a loaded key. The TPM
recovers a seed with the EK, checks an HMAC over the loaded key's
Name, and releases the secret only when the Name matches.

``shirudo attest challenge`` computes the credential on the operator
host. It draws a random 32-byte credential and a seed, which it wraps
with RSA-OAEP under the label ``IDENTITY`` for an RSA EK or derives
from an ephemeral ECDH agreement on P-256 for an ECC EK. From the seed
it derives an AES-128-CFB key and an HMAC key with the TPM's key
derivation function. On the target, ``shirudo attest --challenge``
satisfies the EK's policy with ``TPM2_PolicySecret`` and runs
``TPM2_ActivateCredential``, and the evidence carries the recovered
value. ``attest verify --expect-credential`` compares it. Together with
the attribute checks on the AK, this shows that the key that signed
the evidence is a restricted key fixed in the TPM that holds the EK.

Reset count and clock
---------------------

A TPM keeps a clock and two counters. The reset count increases at
every TPM reset, which a reboot causes. The restart count increases at
a TPM restart, after hibernation, and at a TPM resume, after sleep, and
returns to zero at a TPM reset. ``TPM2_ReadClock`` reads them, and
every quote and certification carries them. For keys outside the
endorsement and platform hierarchies the TPM obfuscates the counts in
signed structures. shirudo's AK is in the endorsement hierarchy, so the
evidence carries the real values.

``init`` records the reset count in its eventlog record, and uses it
to decide whether it has already bound the register in this TPM
period. An extension refuses when the TPM's reset count is not the
recorded one. ``attest verify`` checks that the quote and the
certification carry the same reset count and that it is the one the
init record names. Config activation records both counts.

Device interface
----------------

Linux exposes each TPM as two character devices. ``/dev/tpm0`` is the
raw device. Only one process can have it open at a time, and that
process sees the TPM's real handles and must manage its few slots for
objects and sessions itself. ``/dev/tpmrm0`` goes through the kernel's
resource manager. Any number of processes can open it, and the kernel
swaps objects and sessions in and out so that each open file sees only
its own. The kernel's own TPM users, such as IMA and trusted keys,
reach the TPM without either device.

shirudo talks to the TPM through the tpm2-tss library and the TCTI
named by ``tpm.tcti`` in its authenticated configuration,
``device:/dev/tpmrm0`` by default. While an authenticated image is
active it accepts only a ``device:`` TCTI, so its commands pass through
the character device and the ``tpm_command`` hook, and it ignores the
``SHIRUDO_TPM_TCTI`` environment variable.

Limits
------

The :doc:`/security/threat-model` gives the full analysis of each of
these.

- The transport is not authenticated. shirudo opens the device by path
  in the caller's mount namespace, and its sessions are unsalted and
  unbound, so no TPM response carries proof of its origin. The AK Name
  comparison detects a different TPM but not a device that replays the
  public Name. A root process that substitutes the device can keep
  shirudo's extensions from reaching the real register.
- The owner authorization travels as a password to whatever device is
  at the path, and root on the target can read it from the runtime
  image.
- The ``tpm_command`` guard covers ``TPM2_NV_Extend`` of the register
  only. ``TPM2_NV_UndefineSpace`` with the owner authorization and
  ``TPM2_PCR_Extend`` of PCR 9 or the config PCR remain open to root.
  They cannot rewrite the history, but they stop further extensions,
  fail ``--strict-anchor`` or block config commands until the next
  boot.
- The guard sees commands written to the TPM character devices.
  Kernel-internal TPM use and direct access to the TPM's bus are
  outside it, and every executable in the runtime image runs with
  runtime authority and can extend the register.
- A TPM restart clears the register but not the reset count. ``init``
  then refuses to bind the register again, and extensions fail with
  ``tpm_error`` until the next reboot.
- The algorithms are classical: ECDSA P-256 for the AK, RSA-3072 for
  the writer key, RSA 2048 or P-256 for the EK. Version 185 of the
  TPM 2.0 library specification adds ML-DSA and ML-KEM, and shirudo
  does not use them. See :doc:`pqc`.
- An EK is only as trustworthy as its certificate chain or the
  enrollment pin. Virtual TPMs usually have no EK certificate.
- ``attest verify`` warns but proceeds without ``--ak-name``, so the
  operator has to pin the AK on every verification.

Related
-------

- :doc:`boot` for measured boot, PCR 11 phases and NvPCRs.
- :doc:`/internals/measurement-register` for events and the eventlog.
- :doc:`/operate/attest` to attest a target.
- :doc:`/security/threat-model` for the residual risks.
- `TPM 2.0 Library specification
  <https://trustedcomputinggroup.org/resource/tpm-library-specification/>`__
- `TCG EK Credential Profile
  <https://trustedcomputinggroup.org/resource/http-trustedcomputinggroup-org-wp-content-uploads-tcg-ek-credential-profile/>`__
- `TPM2 PCR Measurements Made by systemd
  <https://systemd.io/TPM2_PCR_MEASUREMENTS/>`__
