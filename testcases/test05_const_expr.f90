! test05_const_expr.f90
! Assignment 9 – Test Case 5: Constant-Expression Subscripts
!
! Demonstrates compile-time constant propagation for arithmetic subscripts.
! The checker folds simple integer arithmetic to catch folded-constant OOB.
!
! Expected diagnostics:
!   error:   DATA(5+6)   → DATA(11), > upper bound 10
!   error:   DATA(2*6)   → DATA(12), > upper bound 10
!   error:   HALF(1-2)   → HALF(-1), < lower bound 0
!   (no error for DATA(3+4) → DATA(7) which is within bounds)

PROGRAM test05_const_expr
    IMPLICIT NONE

    REAL    DATA(10)        ! 1:10
    REAL    HALF(0:7)       ! 0:7
    INTEGER WORK(1:20)      ! 1:20

    ! ── Folded-constant accesses that ARE in bounds ───────────────
    DATA(3+4)   = 1.0    ! folds to 7  — OK (1:10)
    DATA(2*4)   = 2.0    ! folds to 8  — OK (1:10)
    HALF(3-1)   = 0.5    ! folds to 2  — OK (0:7)
    WORK(10+5)  = 99     ! folds to 15 — OK (1:20)

    ! ── Folded-constant accesses that violate bounds ──────────────
    DATA(5+6)   = 99.0   ! folds to 11 — error: > 10
    DATA(2*6)   = 99.0   ! folds to 12 — error: > 10
    HALF(1-2)   = 99.0   ! folds to -1 — error: < 0
    WORK(20+1)  = 99     ! folds to 21 — error: > 20

    PRINT *, "test05 complete"

END PROGRAM test05_const_expr
