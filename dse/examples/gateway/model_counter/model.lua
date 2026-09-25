-- Copyright 2025 Robert Bosch GmbH
--
-- SPDX-License-Identifier: Apache-2.0

-- Counter and linear function:
--   counter = counter + 1
--   output  = input * factor + offset

signal_in = { "input", "factor", "offset" }
signal_out = { "output", "counter" }
idx_in = {}
idx_out = {}


function index_signals()
    for _, signal in ipairs(signal_in) do
        idx_in[signal] = model.sv["in"]:find(signal)
        model:log_notice("signal indexed: %s -> sv[\"in\"].scalar[%d]", signal, idx_in[signal])
    end
    for _, signal in ipairs(signal_out) do
        idx_out[signal] = model.sv["out"]:find(signal)
        model:log_notice("signal indexed: %s -> sv[\"out\"].scalar[%d]", signal, idx_out[signal])
    end
end

function print_signal_values()
    for _, signal in ipairs(signal_in) do
        model:log_debug("signal value: %s = %f", signal, model.sv["in"].scalar[idx_in[signal]])
    end
    for _, signal in ipairs(signal_out) do
        model:log_debug("signal value: %s = %f", signal, model.sv["out"].scalar[idx_out[signal]])
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

    local s_in = model.sv["in"].scalar
    local s_out = model.sv["out"].scalar
    s_out[idx_out.counter] = s_out[idx_out.counter] + 1
    s_out[idx_out.output] = s_in[idx_in.input] * s_in[idx_in.factor] + s_in[idx_in.offset]
    model:log_notice("counter=%f output=%f (input=%f factor=%f offset=%f)",
        s_out[idx_out.counter], s_out[idx_out.output],
        s_in[idx_in.input], s_in[idx_in.factor], s_in[idx_in.offset])
    print_signal_values()

    -- Indicate success.
    return 0
end

function model_destroy()
    model:log_notice("model_destroy()")

    -- Indicate success.
    return 0
end
