#include <stdio.h>

#define MAXN 501
#define NEG_INF (-1e18)

long long dp[MAXN][MAXN]; // dp[i][len] = max sum of increasing subseq of length len, ending at index i
long long a[MAXN];

int main() {
    int n, k;
    scanf("%d %d", &n, &k);

    for (int i = 0; i < n; i++) {
        scanf("%lld", &a[i]);
    }

    // Initialize all states as invalid
    for (int i = 0; i < n; i++)
        for (int len = 0; len <= k; len++)
            dp[i][len] = NEG_INF;

    // Base case: every single element is a subsequence of length 1
    for (int i = 0; i < n; i++) {
        dp[i][1] = a[i];
    }

    // Fill the table
    for (int i = 1; i < n; i++) {
        for (int j = 0; j < i; j++) {
            if (a[j] < a[i]) {
                for (int len = 2; len <= k; len++) {
                    if (dp[j][len-1] != NEG_INF) {
                        if (dp[j][len-1] + a[i] > dp[i][len]) {
                            dp[i][len] = dp[j][len-1] + a[i];
                        }
                    }
                }
            }
        }
    }

    // Find the best answer across all ending positions
    long long ans = NEG_INF;
    for (int i = 0; i < n; i++) {
        if (dp[i][k] > ans) {
            ans = dp[i][k];
        }
    }

    if (ans == NEG_INF)
        printf("-1\n");
    else
        printf("%lld\n", ans);

    return 0;
}