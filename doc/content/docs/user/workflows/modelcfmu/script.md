simulation
channel in
channel out

uses
dse.fmi file:///repo

model direct dse.fmi.example.direct
    channel in in_vector
    channel out out_vector
    envar OFFSET 100
    envar FACTOR 4

workflow generate-modelcfmu
    var FMU_NAME direct
    var FMI_VERSION 2
    var INDEX direct



