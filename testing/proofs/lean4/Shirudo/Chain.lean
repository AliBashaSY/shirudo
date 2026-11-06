-- SPDX-License-Identifier: BSD-2-Clause
-- Copyright (c) 2026 Authors of shirudo

/-!
The measurement register of util/chain.c. Each upatch state change
extends it as `H(register ‖ event)`, the way a TPM extends a PCR, with an
event that hashes a domain-separating tag and its payload. `attest verify`
replays the eventlog and compares the result with the register the TPM
signed. Assuming `H` is collision resistant, modelled as injective on
(register, event) pairs and never yielding the initial value, and the
tagged encoding of events likewise, equal registers mean the target
measured exactly the events the log shows, in that order.
-/

namespace Shirudo.Chain

variable {D K P : Type}

/-- The register after `events`, the newest first. -/
def register (H : D → D → D) (init : D) : List D → D
  | [] => init
  | e :: es => H (register H init es) e

/-- The same register, folded over the events in the order they came. -/
theorem register_eq_foldl (H : D → D → D) (init : D) (events : List D) :
    register H init events = events.reverse.foldl H init := by
  induction events with
  | nil => rfl
  | cons e es ih => simp [register, ih]

/-- Equal registers come from equal event sequences. -/
theorem register_injective (H : D → D → D) (init : D)
    (hH : ∀ a b c d, H a b = H c d → a = c ∧ b = d)
    (hinit : ∀ a b, H a b ≠ init) :
    ∀ l1 l2 : List D, register H init l1 = register H init l2 → l1 = l2
  | [], [], _ => rfl
  | [], f :: fs, h => absurd h.symm (hinit _ f)
  | e :: es, [], h => absurd h (hinit _ e)
  | e :: es, f :: fs, h => by
    have ⟨hr, he⟩ := hH _ _ _ _ h
    rw [he, register_injective H init hH hinit es fs hr]

theorem map_injective {α β : Type} (f : α → β) (hf : Function.Injective f) :
    ∀ l1 l2 : List α, l1.map f = l2.map f → l1 = l2
  | [], [], _ => rfl
  | [], _ :: _, h => by simp at h
  | _ :: _, [], h => by simp at h
  | a :: as, b :: bs, h => by
    simp only [List.map_cons, List.cons.injEq] at h
    rw [hf h.1, map_injective f hf as bs h.2]

/-- Equal registers come from the same tagged events, in the same order:
an event can be neither left out, added, swapped for another nor moved. -/
theorem register_tagged_injective (H : D → D → D) (init : D)
    (encode : K → P → D)
    (hH : ∀ a b c d, H a b = H c d → a = c ∧ b = d)
    (hinit : ∀ a b, H a b ≠ init)
    (henc : ∀ k p k' p', encode k p = encode k' p' → k = k' ∧ p = p')
    (l1 l2 : List (K × P))
    (h : register H init (l1.map fun e => encode e.1 e.2) =
      register H init (l2.map fun e => encode e.1 e.2)) :
    l1 = l2 := by
  have hinj : Function.Injective (fun e : K × P => encode e.1 e.2) := by
    intro ⟨k, p⟩ ⟨k', p'⟩ he
    have ⟨hk, hp⟩ := henc _ _ _ _ he
    subst hk; subst hp; rfl
  exact map_injective _ hinj _ _ (register_injective H init hH hinit _ _ h)

end Shirudo.Chain
