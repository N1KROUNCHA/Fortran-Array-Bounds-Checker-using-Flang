! test01_constant_oob.f90
! Assignment 9 – Test Case 1: Constant Out-of-Bounds Accesses
!
! Expected diagnostics:
!   error: A(0)   — lower bound violation  (declared 1:10)
!   error: A(11)  — upper bound violation  (declared 1:10)
!   error: B(-6)  — lower bound violation  (declared -5:5)
!   error: B(6)   — upper bound violation  (declared -5:5)
!   error: C(3,4) — dim 2 upper bound violation (declared 1:3)

PROGRAM test01_constant_oob
    IMPLICIT NONE

    ! Standard 1-based array
    REAL A(10)              ! bounds: 1:10

    ! Negative lower bound (common in scientific codes)
    INTEGER B(-5:5)         ! bounds: -5:5

    ! 2-D array
    REAL C(5, 3)            ! bounds: 1:5, 1:3

    ! ── Correct accesses (no diagnostics expected) ──────────────
    A(1)  = 1.0
    A(5)  = 2.0
    A(10) = 3.0

    B(-5) = 100
    B(0)  = 200
    B(5)  = 300

    C(1,1) = 0.0
    C(5,3) = 1.0

    ! ── Out-of-bounds accesses (errors expected) ─────────────────

    ! Lower bound violation on A (1-based, index 0 is invalid)
    A(0) = 99.0          ! error: index 0 < lower bound 1

    ! Upper bound violation on A
    A(11) = 99.0         ! error: index 11 > upper bound 10

    ! Lower bound violation on B
    B(-6) = 999          ! error: index -6 < lower bound -5

    ! Upper bound violation on B
    B(6) = 999           ! error: index 6 > upper bound 5

    ! 2-D: dimension 2 upper-bound violation
    C(3,4) = 0.0         ! error: dim-2 index 4 > upper bound 3

END PROGRAM test01_constant_oob
