! test02_variable_index.f90
! Assignment 9 – Test Case 2: Variable Indices (Cannot Verify Statically)
!
! Expected diagnostics:
!   warning: A(i)       — cannot verify i ∈ [1,10]
!   warning: A(i+1)     — cannot verify i+1 ∈ [1,10]
!   warning: B(n)       — cannot verify n ∈ [-5,5]
!   warning: C(i,j)     — cannot verify either subscript
!   (no warning for A(k) inside bounds-checked loop — checker is conservative)

PROGRAM test02_variable_index
    IMPLICIT NONE

    INTEGER, PARAMETER :: N = 10
    REAL A(N)           ! bounds: 1:10
    INTEGER B(-5:5)     ! bounds: -5:5
    REAL C(20, 20)      ! bounds: 1:20, 1:20

    INTEGER i, j, k, m

    ! ── Loop with variable index – cannot verify statically ─────
    ! Even though the loop bounds guarantee safety, the checker
    ! performs no interval analysis on loop induction variables.
    DO i = 1, N
        A(i) = REAL(i)        ! warning: variable index i
    END DO

    ! ── Expression index ─────────────────────────────────────────
    i = 5
    A(i+1) = 0.0              ! warning: expression i+1 (i is not const-propagated across stmts)

    ! ── Negative-lower-bound array with variable ─────────────────
    DO m = -5, 5
        B(m) = m              ! warning: variable index m
    END DO

    ! ── 2-D with two variable subscripts ─────────────────────────
    DO i = 1, 20
        DO j = 1, 20
            C(i,j) = 0.0     ! warning: dim-1 variable i, dim-2 variable j
        END DO
    END DO

    ! ── Variable that happens to be in range (checker still warns)─
    k = 7
    A(k) = 3.14              ! warning: variable index k (value 7 not tracked)

END PROGRAM test02_variable_index
