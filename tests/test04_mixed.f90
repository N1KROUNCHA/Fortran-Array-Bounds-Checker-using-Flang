! test04_mixed.f90
! Assignment 9 – Test Case 4: Mixed Constant and Variable Accesses
!
! Demonstrates the checker handling both definite violations (errors)
! and unverifiable variable indices (warnings) in one compilation unit.
!
! Expected diagnostics:
!   error:   BUF(0)      — lower bound violation (1:256)
!   error:   BUF(257)    — upper bound violation (1:256)
!   error:   GRID(0,5)   — dim-1 lower bound violation (1:100, 1:100)
!   error:   STACK(-1)   — lower bound violation (0:63)
!   warning: BUF(pos)    — variable pos, cannot verify
!   warning: GRID(r,c)   — variable r,c, cannot verify

PROGRAM test04_mixed
    IMPLICIT NONE

    ! Typical buffer — 1-based
    INTEGER BUF(256)        ! 1:256

    ! 2-D simulation grid
    REAL GRID(100, 100)     ! 1:100 × 1:100

    ! Zero-based stack
    REAL STACK(0:63)        ! 0:63

    INTEGER pos, r, c, idx

    ! ── Correct constant accesses (no diagnostics) ───────────────
    BUF(1)     = 0
    BUF(128)   = 1
    BUF(256)   = 2

    GRID(1,1)     = 0.0
    GRID(50,50)   = 1.0
    GRID(100,100) = 2.0

    STACK(0)  = 0.0
    STACK(32) = 0.5
    STACK(63) = 1.0

    ! ── Definite constant violations (errors) ────────────────────
    BUF(0)   = -1       ! error: 0 < 1 (lower bound)
    BUF(257) = -1       ! error: 257 > 256 (upper bound)

    GRID(0,5) = 0.0     ! error: dim-1 index 0 < 1

    STACK(-1) = 0.0     ! error: -1 < 0 (lower bound)

    ! ── Variable accesses (warnings) ─────────────────────────────
    READ *, pos
    BUF(pos) = 42       ! warning: pos unverifiable

    DO r = 1, 100
        DO c = 1, 100
            GRID(r, c) = REAL(r * c)  ! warning: r and c variable
        END DO
    END DO

END PROGRAM test04_mixed
