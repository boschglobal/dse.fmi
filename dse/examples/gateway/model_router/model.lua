-- Copyright 2025 Robert Bosch GmbH
--
-- SPDX-License-Identifier: Apache-2.0

SIG_COUNTER = 1
signal_in= {
    [SIG_COUNTER] = "in",
}
signal_out = {
    [SIG_COUNTER] = "out",
}
signals = {}


function index_signals()
    for idx, signal in pairs(signal_in) do
        local sv_idx = model.sv["in"]:find(signal)
        signals[idx] = sv_idx
        model:log_notice("signal indexed: [%d]%s -> sv[\"in\"].scalar[%d]", idx, signal, sv_idx)
    end
    for idx, signal in pairs(signal_out) do
        local sv_idx = model.sv["out"]:find(signal)
        signals[idx] = sv_idx
        model:log_notice("signal indexed: [%d]%s -> sv[\"out\"].scalar[%d]", idx, signal, sv_idx)
    end
end

function print_signal_values()
    for idx, signal in pairs(signal_in) do
        model:log_debug("signal value: [%d]%s = %f", idx, signal, model.sv["in"].scalar[signals[idx]])
    end
    for idx, signal in pairs(signal_out) do
        model:log_debug("signal value: [%d]%s = %f", idx, signal, model.sv["out"].scalar[signals[idx]])
    end
end


function model_create()
    model:log_notice("model_create()")
    model:log_notice("  step_size: %f", model:step_size())

    -- Debugging info.
    model:log_debug(tostring(model))

    -- Index signals used by this model.
    index_signals()

    -- Indicate success.
    return 0
end

function model_step()
    model:log_notice("model_step() @ %f", model:model_time())

    -- Increment the counter signal.
    model.sv["out"].scalar[signals[SIG_COUNTER]] = model.sv["in"].scalar[signals[SIG_COUNTER]]
    print_signal_values()

    -- Indicate success.
    return 0
end

function model_destroy()
    model:log_notice("model_destroy()")

    -- Indicate success.
    return 0
end
