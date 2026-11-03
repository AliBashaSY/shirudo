..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: Tutorial
:audience: First-time users

Quickstart
==========

In this tutorial you build shirudo from a checkout, boot a protected test
target in a virtual machine and take one upatch through its whole life:
you sign it on the operator side, load it on the target, check with the
target's TPM what the target runs, and unload it again.

Your host plays the operator. The test VM plays the target: it boots the
way a production target does and enforces the same policy.

Before you begin
----------------

- A Linux host on x86-64 or arm64 with KVM. ``/dev/kvm`` must be readable
  and writable.
- Root on that host. The test VM builds its initrd with dracut, which needs
  root, and the signing key it generates for the VM belongs to root. Run
  every host command on this page as root.
- Network access. The first boot clones the bpf-next kernel tree and
  downloads an Ubuntu cloud image.
- To build shirudo: a C toolchain with ``make`` and ``pkg-config``, a
  clang and LLVM 23 with the BPF target, ``clang`` installed as
  ``/usr/bin/clang``, and the development files of OpenSSL 3.5 or later,
  libelf, zlib, libcurl 7.75 or later, tpm2-tss (``tss2-esys`` 4.0 or later
  and ``tss2-tctildr``) and LuaJIT.
- For the test VM, with their Debian and Ubuntu names: QEMU
  (``qemu-system-x86`` or ``qemu-system-arm``), ``qemu-utils``, ``swtpm``,
  ``cloud-image-utils``, ``libguestfs-tools``, ``dracut-core``,
  ``cryptsetup-bin``, ``e2fsprogs``, ``openssh-client``, ``iproute2``,
  ``isc-dhcp-client``, ``curl``, ``kmod`` and ``file``, and the static
  archives the target binary links: libelf, zlib, zstd, LuaJIT and a static
  glibc. The VM scripts stop at the first missing command and print the
  command that installs it. The kernel build installs its own build
  dependencies.
- ``jq`` and the ``openssl`` command, for the attestation step.

:doc:`/operate/plan/system-requirements` lists the requirements in full.

Build shirudo
-------------

1. In the root of your checkout, build the ``shirudo`` binary:

   .. code-block:: console

      operator$ make

2. Build the operator helper:

   .. code-block:: console

      operator$ make shirudo-build

   ``shirudo`` is the command you run on both sides. Operator commands
   such as ``shirudo build`` hand their work to the helper
   ``tools/shirudo-build/shirudo-build``, which compiles and signs. The
   signing code lives only in the helper, so it never reaches a target.

3. Put both on your ``PATH`` for this shell:

   .. code-block:: console

      operator$ export PATH="$PWD:$PWD/tools/shirudo-build:$PATH"

   When no helper is installed, ``shirudo`` finds it on ``PATH``.

Boot a test target
------------------

1. Boot a test VM named ``quickstart``:

   .. code-block:: console

      operator$ make -C testing vm VM_NAME=quickstart
      ...
      [test-vm] building an initrd with shirudo for 7.3.0-bpf-next+
      [test-vm] booting bzImage with 4 CPUs and 4G memory
      [test-vm] VM ready: name=quickstart state=.../build/test-vm/vms/quickstart qemu_pid=... swtpm_pid=...
      ...
      [test-vm] console: make -C testing vm-console VM_NAME=quickstart
      [test-vm] ssh: make -C testing vm-ssh

   The first run takes a while. It builds a bpf-next kernel with the
   patches under :src:`kernel/`, an Ubuntu disk image and a bundle: the static
   target binary, a dm-verity runtime image, signed config images and a
   throwaway signing key. Later runs reuse them.

   The VM boots the way a production target does. The kernel starts with
   the BPF LSM and with the ``.bpf`` and ``.dm-verity`` keyrings open for
   enrollment. ``shirudo init`` runs from the initrd: it mounts the runtime
   image and the signed config image, enrolls the bundle's certificates in
   the ``.bpf`` keyring and seals it, attaches its LSM programs and binds
   the measurement register in the TPM, here a software TPM (``swtpm``).
   The root file system then runs ``init`` a second time and takes over.
   One thing differs from production: QEMU starts the kernel and initrd
   directly, not from a signed unified kernel image, so nothing measures
   the boot phases into PCR 11.

2. Open a root shell on the target:

   .. code-block:: console

      operator$ make -C testing vm-ssh VM_NAME=quickstart

   Commands with the ``target#`` prompt run in this shell.

3. Make ``shirudo`` name the trusted binary in this shell:

   .. code-block:: console

      target# alias shirudo=shirudo-runtime

   On a target, the trusted ``shirudo`` is the binary on the runtime image,
   ``/run/shirudo/runtime/usr/bin/shirudo``, and the test VM wraps it as
   ``shirudo-runtime``. The LSM gives access to shirudo's state only to
   tasks that executed that binary from the runtime image. The test VM
   keeps an identical copy under ``/usr/local/bin``. That copy prints its
   version, but every command that reads shirudo's state stops:

   .. code-block:: console

      target# /usr/local/bin/shirudo status
      shirudo: image state: Permission denied

4. Change to your checkout inside the guest:

   .. code-block:: console

      target# cd /workspace/shirudo

   The test VM shares your checkout with the guest, which sees it at
   ``/workspace/shirudo``. A file you build under the checkout on the host
   shows up there. Files you change in the guest stay in the guest.

Check enforcement
-----------------

1. Read the status report:

   .. code-block:: console

      target# shirudo status
      Host:              quickstart
      Roles:             target
      Mode:              production
      Status:            ok
      Version-ID:        447b34bc8bc5547334ced4c737f987a9d3f8f03e332cf234626f0b81b5955064

      Conditions:
        TYPE             STATUS  DETAIL
        LsmAttached      OK      lsm/bpf link present and pinned
        KeyringSealed    OK      bpf serial=130847582, dm-verity serial=962472513, pins match
        ChainIntact      OK      live register == eventlog replay
        TpmMeasured      OK      NV index 0x01818E00 readable
        ConfigActive     OK      config dc90bacc356f.. is active

      Tamper attempts (refused this boot):
        LoadNotLoader            15  program load that is not a signed loader
        MapCreate                 1  map creation by another executable
        BpfCommand                8  detach, update, delete or unclassified bpf() command
        SealPtrace                6  ptrace of a shirudo task

      Keyring (.bpf):
        Serial:                130847582
        Frozen serial:         130847582  (matches)
        Certificates:          2 trusted, 0 revoked, 0 unvouched, 2 in the bpf keyring
          d1bc43c91b9dabf4cd6f3ddf08a177927b3dac8ca833ddc496167253761b4497  (primary)
            CN=shirudo, ML-DSA-87, issued 2026-10-29
            key 199275792  shirudo: 6ec99b4c3d1c51e461ce1299fae6ab87d212ce9c  (frozen)
          c1c30b07f86808e81ae1c0458c926f00b85bcc815d371d23d7683903f2d3266e
            CN=shirudo, ML-DSA-87, issued 2026-10-29
            key 106884895  shirudo: c350bd270c426c5760b202062e15e274ea30f45d  (frozen)

      Keyring (.dm-verity):
        Serial:                962472513
        Frozen serial:         962472513  (matches)
        Certificates:          1 trusted, 1 in the dm-verity keyring
          d1bc43c91b9dabf4cd6f3ddf08a177927b3dac8ca833ddc496167253761b4497  (config)
            CN=shirudo, ML-DSA-87, issued 2026-10-29
            key 200951639  shirudo: 6ec99b4c3d1c51e461ce1299fae6ab87d212ce9c  (frozen)

      upatches loaded (0):

      TPM Measurement:
        TPM:               reachable (device:/dev/tpmrm0)
        NV index:          0x01818E00 (anchored in PCR 9)
        Chain:             dc21c3dabcd2fd0a751c7146ad71c595f2386a4406feb64c3d6e2bd65c5bc706
        Register name:     000b9b7b0f925415fdf0d5676369f8cc9146b9e3142537148b31e79dbffcc96e72f8
        Anchored:          yes (name on record)
        PCR before init:   0000000000000000000000000000000000000000000000000000000000000000
        Phase at init:     0000000000000000000000000000000000000000000000000000000000000000 (PCR 11)
        Bound in:          initrd
        Chain vs eventlog: matches
        Eventlog:          /var/lib/shirudo/eventlog  (1 entries, 1975 bytes)

      Assets (/var/lib/shirudo):
        keys/cert.der          present
        keys/signing_key.pem   absent
        eventlog               present (1975 bytes)
        upatches/              absent
        incoming/              absent
        bpffs/                 mounted

   Your serials, digests and counts differ: the bundle generates its own
   keys, and the TPM and the guest are your own. Look at these parts:

   - ``Status: ok`` with every condition ``OK``. Scripts read the verdict
     from ``shirudo status --json``, because the command exits with 0
     whatever the verdict is.
   - ``Mode: production``: the LSM link is pinned.
   - The keyring: the frozen serial matches the live one, and the ``.bpf``
     keyring holds two certificates. The primary one belongs to the
     bundle's signing key. The other is a second certificate, vouched for
     by the first, which the test VM enrolls to exercise a set of keys.
   - The TPM measurement: ``Bound in: initrd`` and ``Chain vs eventlog:
     matches``. ``Chain`` is the register's value, and the eventlog holds
     one entry, the boot event.
   - The tamper attempts: calls the policy refused since boot. Other
     software on the guest has already made some.

2. Try to read one of shirudo's pins as root:

   .. code-block:: console

      target# cat /run/shirudo/bpffs/tamper
      cat: /run/shirudo/bpffs/tamper: Permission denied

   Root without runtime authority cannot open shirudo's maps and links.
   The next status report counts the attempt under ``ObjGet``.

Build and sign an upatch
------------------------

The test VM's bundle generated an ML-DSA-87 key pair for this VM, and the
VM enrolled its certificate. You sign with that key on the host.

1. Name the key directory, here for the default bpf-next kernel:

   .. code-block:: console

      operator$ KEYS=$PWD/build/test-vm/host-bundle-bpf-next-verity.operator/keys

2. Look at the upatch you are going to sign,
   :src:`testing/fixtures/basic.bpf.c`:

   .. code-block:: c

      // SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
      #include "vmlinux.h"
      #include <bpf/bpf_helpers.h>
      #include <bpf/bpf_tracing.h>

      char _license[] SEC("license") = "Dual BSD/GPL";

      SEC("fentry/do_nanosleep")
      int BPF_PROG(shirudo_test_basic)
      {
              return 0;
      }

   It attaches to the kernel function ``do_nanosleep()`` and does nothing,
   which makes it a safe first load.

3. Build and sign it:

   .. code-block:: console

      operator$ shirudo build testing/fixtures/basic.bpf.c --keys-dir "$KEYS" --vmlinux vendor/linux/vmlinux.h -o basic.upatch
      wrote basic.upatch (insns=992 sig=4853 data=2456 sections=2 maps=0 progs=1 decoders=0 name=basic)

   The helper compiles the program with clang against the kernel types in
   ``vendor/linux/vmlinux.h``, turns the object into a loader program that
   performs the load on the target, and signs the loader and its data with
   your key. The upatch takes its name, ``basic``, from the file name.
   The sizes and the digests on this page depend on your toolchain and on
   the path of the source, so yours differ.

4. Inspect the artifact:

   .. code-block:: console

      operator$ shirudo upatch info basic.upatch
      File:                  basic.upatch
      Name:                  basic
      Type:                  LOAD
      Payload type:          bpf
      Version:               1
      Payload SHA256:        46f3d9f5f6568bddb7692cad28e99a86a3104192d8a2251f9ef9ec6d593c2035
      File size:             8661 bytes

      Loader:
        Instructions:          off=360 len=992
        Log buffer size:       1048576
        Metadata:              (none)
        Data:                  off=6205 len=2456

      Signature:
        Offset:                1352
        Length:                4853 bytes

      Signed sections:
        Name:                  basic
        Attach metadata:       176 bytes
        Unload payload:        (none)

      Decoders:
        (none)

   ``Payload SHA256`` identifies what the upatch loads. The target records
   it with the load, and the register measures it.

5. Keep the payload digest:

   .. code-block:: console

      operator$ PAYLOAD=$(shirudo upatch info --json basic.upatch | jq -r .payload_sha256)

6. Build the unload directive for this payload:

   .. code-block:: console

      operator$ shirudo build --unload basic --payload-sha256 "$PAYLOAD" --keys-dir "$KEYS" -o basic.unload.upatch
      wrote basic.unload.upatch (insns=1320 sig=4853 data=2808 sections=2 maps=1 progs=1 decoders=0 name=basic UNLOAD)

   An unload is a signed artifact too, and the target runs it through the
   same signature check as a load. The directive names the upatch and the
   payload it removes: if another payload is loaded under the name
   ``basic``, the target refuses it with ``payload_mismatch``.

Load it
-------

1. Load the upatch on the target:

   .. code-block:: console

      target# shirudo upatch load basic.upatch
      loaded signed loader: prog_fd=10 keyring=531121468 sig=4853B insns=124 data=2456B
      loader ok, retval=0 (maps=0 progs=1)
      attached + pinned prog 0 (raw_tracepoint) -> /run/shirudo/bpffs/upatches/basic/link
      upatch load: name=basic keyring=531121468 progs=1
      upatch load: chain extended, value=ff0eb201e073fdeeba80a4b268f039bea58ec6dd2998e6651c4834ee99f3e103
      upatch load measured: name=basic chain=ff0eb201e073fdeeba80a4b268f039bea58ec6dd2998e6651c4834ee99f3e103

   The kernel checked the loader's signature against the ``.bpf`` keyring
   (the serial from the status report) before it accepted the load. The
   loader then ran in the kernel and created the program, and shirudo
   attached it and pinned the link in its private bpffs. Last, shirudo
   extended the measurement register with the load and appended the event
   to the eventlog.

2. List the upatches:

   .. code-block:: console

      target# shirudo upatch list
      upatches (/var/lib/shirudo/upatches):
        loaded  type    kind    payload-sha256      name
        yes     load    bpf     46f3d9f5f6568bdd..  basic

   The target keeps a copy of the artifact and loads it again at the next
   boot.

3. Read the status report again:

   .. code-block:: console

      target# shirudo status
      ...
      upatches loaded (1):
        NAME                      LOCATION                       MAPS  STATUS                        AGE
        basic                     fentry/do_nanosleep               0  success                       0s

      TPM Measurement:
        ...
        Chain:             ff0eb201e073fdeeba80a4b268f039bea58ec6dd2998e6651c4834ee99f3e103
        ...
        Eventlog:          /var/lib/shirudo/eventlog  (2 entries, 2311 bytes)
      ...

   The register moved from its boot value to the value the load printed,
   and the eventlog holds the boot event and the load.

Verify with the TPM
-------------------

The status report is what the target says about itself. Evidence signed
by its TPM lets you check the same facts on the operator side, with
nothing but the evidence, your certificates and your artifacts.

1. Make a fresh nonce on the operator host:

   .. code-block:: console

      operator$ NONCE=$(openssl rand -hex 32)
      operator$ echo "$NONCE"
      407c16fda5701c6159662135fb40979a44f7e3b048fef2b73f84fff83ff91083

2. Ask the target for evidence that answers this nonce:

   .. code-block:: console

      target# shirudo attest --nonce=<nonce> --out=/mnt/shirudo-out/ev.json

   ``<nonce>`` is the value you printed. The TPM signs a quote over the
   PCRs and a certification of the register with its attestation key, and
   the document adds this boot's eventlog. ``/mnt/shirudo-out`` is a
   directory the test VM shares with the host, as
   ``build/test-vm/vms/quickstart/result``.

3. Name the evidence and the second enrolled certificate on the host:

   .. code-block:: console

      operator$ EV=build/test-vm/vms/quickstart/result/ev.json
      operator$ OWN=build/test-vm/host-bundle-bpf-next-verity/fixtures/own.der

4. Predict the register from what you know:

   .. code-block:: console

      operator$ shirudo attest predict --cert="$KEYS/cert.der" --cert="$OWN" --event=load:basic="$PAYLOAD"
      set_digest: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
      init_event: cb206abf8e514bc714fc69f372d464d2c7512f1489431aed2319551c5ed34b9a
      register_at_boot: d1bdb80f7c0cfc1560f0a5a0b974d282bfd05fc95852b7649d59a054b450aa51
      register_after_events: ff0eb201e073fdeeba80a4b268f039bea58ec6dd2998e6651c4834ee99f3e103

   ``register_at_boot`` is the ``Chain`` of the first status report and
   ``register_after_events`` the ``Chain`` after the load. The boot event
   covers the set of certificates ``init`` enrolled, so the prediction
   takes every enrolled certificate: the test VM enrolls two, the bundle's
   ``cert.der`` and ``own.der``. It also covers the upatches restored at
   boot, none here, which is why ``set_digest`` is the SHA-256 of empty
   input.

5. Verify the evidence against the prediction:

   .. code-block:: console

      operator$ shirudo attest verify --evidence="$EV" --nonce="$NONCE" --ak-name="$(jq -r .ak.name "$EV")" --expect-chain=<register-after-events>
      ok:   nonce matches the request
      ok:   attestation key: restricted, fixed to this TPM
      ok:   attestation key: name matches its public area
      ok:   attestation key: the pinned key
      ...
      ok:   quote: ECDSA signature by the attestation key
      ...
      ok:   register: certified contents match the listed value
      ok:   quote and certification are of one TPM reset
      ok:   config: config root history matches the quoted PCR
      note: config root has no operator approval; use --expect-config-root with --expect-config-pcr
      note: config records a selection; direct TPM writer protection is deferred
      ok:   eventlog: this boot has an init record
      ...
      ok:   eventlog: 2 records replay to the certified value
      ok:   anchor: record digest matches the index name
      ok:   anchor: PCR 9 equals the recorded value extended by the anchor
      ok:   state: register equals the expected value
      note: event after boot: upatch_load basic
      verdict: ok

   ``<register-after-events>`` is the last line of the prediction. The
   checks prove that the target's TPM signed the evidence for your nonce,
   that the eventlog replays to the register value the TPM certified, and
   that this value is the one you predicted. The command exits with 0 on
   ``verdict: ok`` and with 1 on ``verdict: FAIL``. The notes name checks
   this page leaves out, such as the approved config image.
   :doc:`/operate/run/attest` covers them.

   Here you took the attestation key's name from the evidence itself,
   which proves nothing about which TPM signed it. :doc:`guides/attest-target`
   shows how to pin the key once, with a challenge only the target's TPM
   can answer.

Unload it
---------

1. Unload the upatch:

   .. code-block:: console

      target# shirudo upatch unload basic.unload.upatch
      loaded signed loader: prog_fd=10 keyring=531121468 sig=4853B insns=165 data=2808B
      upatch unload: payload matches for 'basic'
      upatch unload: chain extended, value=fe7bd139c02aca0d6e5efc3940c1cd902adbe676b7875be4c73c878b3f5212b8
      upatch unload measured: name=basic chain=fe7bd139c02aca0d6e5efc3940c1cd902adbe676b7875be4c73c878b3f5212b8
      upatch unload: detached + unpinned basic

   The target ran the signed directive, checked that the loaded payload is
   the one it names, measured the unload, then detached the program.

2. List the upatches:

   .. code-block:: console

      target# shirudo upatch list
      upatches (/var/lib/shirudo/upatches):
        (none persisted)

   The register did not return to its boot value. It moved forward to a
   third value, and the eventlog records the boot, the load and the
   unload. The register only starts over when the TPM resets, at the next
   boot. To predict it now, give ``attest predict`` both events:
   ``--event=load:basic="$PAYLOAD" --event=unload:basic="$PAYLOAD"``.

Stop the VM
-----------

The quick guides continue with this VM. Keep it running if you go on with
them.

1. Leave the target shell:

   .. code-block:: console

      target# exit

2. Stop the VM:

   .. code-block:: console

      operator$ make -C testing vm-stop VM_NAME=quickstart

   The VM's disk, TPM state and logs stay under
   ``build/test-vm/vms/quickstart``. A stopped VM does not boot again under
   the same name.

3. Remove its state when you no longer need it:

   .. code-block:: console

      operator$ make -C testing vm-clean VM_NAME=quickstart

Next steps
----------

- :doc:`guides/first-upatch` writes an upatch of your own.
- :doc:`guides/publish-package` ships an upatch through an OCI registry.
- :doc:`guides/attest-target` enrolls the target's TPM and verifies its
  evidence.
- :doc:`how-it-works` explains each step of this page.
- :doc:`install` installs shirudo on real hosts, and
  :doc:`/operate/deploy/boot-integration` brings a target up.
