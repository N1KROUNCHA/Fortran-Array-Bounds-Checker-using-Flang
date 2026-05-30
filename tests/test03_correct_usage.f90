! test03_correct_usage.f90
! Assignment 9 – Test Case 3: Correct Accesses (No Diagnostics)
!
! All constant-index accesses are within declared bounds.
! The checker should emit ZERO errors or warnings for constant indices.

PROGRAM test03_correct_usage
    IMPLICIT NONE

    ! 1-D arrays with various lower bounds
    REAL              :: STD(10)        ! 1:10
    REAL              :: NEG(-5:75)      ! -5:5
    REAL              :: OFF(0:99)      ! 0:99
    INTEGER           :: MAT(4, 4)      ! 1:4, 1:4

    ! ── All constant-index accesses within bounds ────────────────

    ! Standard 1-based
    STD(1)  = 0.0
    STD(5)  = 1.0
    STD(10) = 2.0

    ! Negative-lower-bound
    NEG(-5) = -1.0
    NEG(-1) = 0.0
    NEG(0)  = 0.5
    NEG(5)  = 1.0

    ! Zero-based
    OFF(0)  = 0.0
    OFF(50) = 0.5
    OFF(99) = 1.0

    ! 2-D: all four corners plus centre
    MAT(1,1) = 1
    MAT(5,4) = 2
    MAT(4,1) = 3
    MAT(4,4) = 4
    MAT(2,3) = 5

    PRINT *, "All accesses verified safe at compile time."

END PROGRAM test03_correct_usage
