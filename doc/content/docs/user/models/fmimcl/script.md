simulation
channel in
channel out

uses
dse.fmi file:///repo

model fmu dse.fmi.example.fmu
    channel in in_vector
    channel out out_vector

workflow generate-fmimcl
    var FMU_NAME foo
    var FMI_VERSION 2
