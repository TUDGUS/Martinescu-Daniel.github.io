#include <stdio.h>

#define MOD 1000000007
#define MAXN 1000001

long long dp[MAXN];

int main() {
    int n;
    scanf("%d", &n);

    dp[0] = 1;
    dp[1] = 1;

    for (int i = 2; i <= n; i++) {
        dp[i] = (dp[i-1] + dp[i-2]) % MOD;
    }

    printf("%lld\n", dp[n]);
    return 0;
}