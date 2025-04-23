/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_DATAFRAME_INDEX_H_
#define SRC_TRACE_PROCESSOR_DATAFRAME_INDEX_H_

#include <cstdint>

#include "src/trace_processor/dataframe/impl/query_plan.h"
#include "src/trace_processor/dataframe/impl/slab.h"
#include "src/trace_processor/dataframe/impl/static_vector.h"

namespace perfetto::trace_processor::dataframe {

// Represents an index on a dataframe and can be used to speed up
// queries on the dataframe.
class Index {
 public:
 private:
  // The positions of the columns covered by this index inside the
  // dataframe.
  impl::FixedVector<uint32_t, impl::kMaxColumns> columns_;

  // The permuation vector of the rows indices in the dataframe
  // sorted by all the columns specified by `columns_`.
  impl::Slab<uint32_t> perutation_vector_;
};

}  // namespace perfetto::trace_processor::dataframe

#endif  // SRC_TRACE_PROCESSOR_DATAFRAME_INDEX_H_
