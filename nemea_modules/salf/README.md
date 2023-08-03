This module performs streaming active learning. It is based on the paper "Active Learning with Evolving Streaming Data" of Zliobaite at al.

Compile: gcc salf.c -lunirec -ltrap -lnemea-common -g -o salf

The active learning logic is performed in the function `sqs (flow, config)` (sqs = streaming query strategy), where flow is IP flow accepted by input interface and config is configuration of the module. The function returns `True` if the flow should be potentionaly labeled and `False` otherwise.

The pseudocode for this module:

```
config = parse_args()
for each flow from input interface:
    if sqs(flow, config):
        label(flow)
    else:
        continue
```

The implementation of the `sqs` funtion is given by config. The configuration should be passed from the command line. The base configuration is given by NEMEA [1,2]. The SALF module specific configuration is given by the following parameters:
 1. `--query-strategy` - name of the query strategy to be used. Different strategies expects different parameters.
 2. `--budget` - every strategy is limited by budget. This parameter specifies the budget. This number should be in interval [0,1] and it is interpreted as percentage of the data. For example, if the budget is 0.1, then the strategy can label MAX only 10% of the data, but could label less.

For the begining, implement `random` strategy. This strategy labels data randomly. The pseudocode for this strategy is:

```
if random() < budget:
    return True
else:
    return False
```


It is NEMEA module [1] based on NEMEA system [2].

[1] https://github.com/CESNET/Nemea-Modules

[2] https://github.com/CESNET/Nemea