simulation
channel in
channel out

uses
dse.fmi file:///repo

model gateway dse.fmi.example.gateway
    channel in E2M_M2E
    channel out com_phys

workflow generate-gatewayfmu
    var FMU_NAME gateway
    var FMI_VERSION 2
    var STACK stack.yaml
