###############################################################################
# This script takes a set of data obtained from runtime, and performs basic   #
# linear regression.                                                          #
###############################################################################

# Load the data
runtime_stats = read.csv("test.csv");

# Perform the regression
# TODO: 1) covariance; 2) all factors
regression_fit = lm(latency ~ dcache_miss + dtlb_miss + remote_numa_access +
                    core_frequency,
                    data=runtime_stats);

# Summarize the regression result
summary(regression_fit);
