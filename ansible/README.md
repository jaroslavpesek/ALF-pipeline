In this folder you can find the ansible playbooks to deploy the whole system.

Components of the probe:
- ML module (predictor)
- SALF NEMEA module providing streaming active learning
- DP3 system
- feeder DP3 module feeding the data from SALF for DP3 buffer to process
- Annotator module (DP3 module for annotation)
- QoD module (DP3 module for QoD calculation)
- Exporter module (export labeled data from DP3 to .h5 database)
- ML deployer module - train model based on .h5 database and deploy it to the ML module