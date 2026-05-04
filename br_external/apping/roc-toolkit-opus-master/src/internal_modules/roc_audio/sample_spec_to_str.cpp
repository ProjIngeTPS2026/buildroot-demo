/*
 * Copyright (c) 2023 Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "roc_audio/sample_spec_to_str.h"

namespace roc {
namespace audio {

sample_spec_to_str::sample_spec_to_str(const SampleSpec& sample_spec) {
    core::StringBuilder bld(buf_, sizeof(buf_));

    format_sample_spec(sample_spec, bld);
}

} // namespace audio
} // namespace roc
