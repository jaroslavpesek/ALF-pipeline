# SALF -

## Description
This module performs streaming active learning. It is based on the paper "Active Learning with Evolving Streaming Data" of Zliobaite at al.

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


## Interfaces
- Input: 1
- Output: 1

## Parameters
### Parameters of module

- `-b  --budget <int32>`           Every strategy is limited by budget. This parameter specifies the budget. This number should be in interval [0,1] and it is interpreted as percentage of the data. For example, if the budget is 0.1, then the strategy can label MAX only 10% of the data, but could label less.

- `-q  --query-strategy <int32>`   ID of the query strategy (sqs function) to be used. Different strategies expects different parameters.
    -   `0`  Random Strategy
    -   `1`   Fixed Uncertainty
    -   `2`  Variable Uncertainty Strategy
    -   `3`  Uncertainty Strategy with Randomization

- `-t  --threshold <double>`     Labeling threshold for Fixed uncertainty strategy.

- `-s  --step <double>`           Adjusting step for Variable Uncertainty Strategy and Uncertainty Strategy with Randomization.

- `-d  --deviation <double>`      Standard deviation of the threshold radomization used in Uncertainty Strategy with Randomization

- `-n  --no-eof`                  Do not send terminate message vie output IFC.



### Common TRAP parameters
- `-h [trap,1]`        Print help message for this module / for libtrap specific parameters.
- `-i IFC_SPEC`      Specification of interface types and their parameters.
- `-v`               Be verbose.
- `-vv`              Be more verbose.
- `-vvv`             Be even more verbose.
