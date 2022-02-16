#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
	// A at 0x0010d0a0, B at 0x0014d0a0
	if (M == 32 && N == 32) { // unique trans function designed for 32x32 matrix.
		int i, j, x, y, tmp;

		for (x = 0; x < 32; x += 8) {
			for (y = 0; y < 32; y += 8) {
				// step of silding window = 8
				for (i = 0; i < 8; ++i) {
					for (j = 0; j < 8; ++j) {
						// treat blocks at diagonal specially
						if (x == y && i == j)
							continue;
						tmp = A[x + i][y + j];
						B[y + j][x + i] = tmp;
					}
					// now transfer diagonal
					if (x == y) {
						tmp = A[x + i][y + i];
						B[y + i][x + i] = tmp;
					}
				}
			}
		}
	}

	if (M == 64 && N == 64) { // unique trans function designed for 64x64 matrix.
		int i, j, x, y, tmp;

		for (x = 0;x < 64;x += 8) {
			// deal with diagnal
			// store 8x8 diagnal matrix into unused caches
			for (i = 0;i < 4;i++) {
				for (j = 0;j < 8;j++) {
					tmp = A[x + i][x + j];
					B[x + i][(x + 8) % 64 + j] = tmp;
				}
			}
			for (i = 4;i < 8;i++) {
				for (j = 0;j < 8;j++) {
					tmp = A[x + i][x + j];
					B[x + i][(x + 16) % 64 + j] = tmp;
				}
			}
			for (i = 0;i < 4;i++) {
				for (j = 0;j < 4;j++) {
					tmp = B[x + j][(x + 8) % 64 + i];
					B[x + i][x + j] = tmp;
				}
			}
			for (i = 0;i < 4;i++) {
				for (j = 4;j < 8;j++) {
					tmp = B[x + j][(x + 16) % 64 + i];
					B[x + i][x + j] = tmp;
				}
			}
			for (i = 4;i < 8;i++) {
				for (j = 0;j < 4;j++) {
					tmp = B[x + j][(x + 8) % 64 + i];
					B[x + i][x + j] = tmp;
				}
			}
			for (i = 4;i < 8;i++) {
				for (j = 4;j < 8;j++) {
					tmp = B[x + j][(x + 16) % 64 + i];
					B[x + i][x + j] = tmp;
				}
			}
			// other 8x8 matrix
			for (y = 0;y < 64;y += 8) {
				if (x == y) continue;
				for (j = 0;j < 8;j++) {
					for (i = 0;i < 4;i++) {
						tmp = A[y + j][x + i];
						B[x + i][y + j] = tmp;
					}
				}
				for (j = 7;j >= 0;j--) {
					for (i = 4;i < 8;i++) {
						tmp = A[y + j][x + i];
						B[x + i][y + j] = tmp;
					}
				}
			}
		}
	}
	if (M == 61 && N == 67) { // unique trans function designed for 61x67 matrix.
		int i, j, x, y, tmp;

		// 56x64 submatrix
		for (y = 0;y < 64;y += 8) {
			for (x = 0;x < 56;x += 8) {
				for (i = 0;i < 8;i++) {
					for (j = 0;j < 8;j++) {
						if (i == j) continue;
						tmp = A[x+i][y+j];
						B[y+j][x+i] = tmp;
					}
					tmp = A[x+i][y+i];
					B[y+i][x+i] = tmp;
				}
			}
		}
		// foot 5x64 submatrix
		for (y = 0;y < 64;y+=8) {
			for (i = 56;i < 61;i++) {
				for (j=0;j<8;j++) {
					tmp = A[i][y+j];
					B[y+j][i] = tmp;
				}
			}
		}
		// right side 61x3 submatrix
		for (i = 0;i < 61;i++) {
			for (j = 64;j < 67;j++) {
				tmp = A[i][j];
				B[j][i] = tmp;
			}
		}
	}
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }
}





/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 
}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

