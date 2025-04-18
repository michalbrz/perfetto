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

#ifndef SRC_TRACED_PROBES_FROZEN_FTRACE_FTRACE_DATA_SOURCE_H_
#define SRC_TRACED_PROBES_FROZEN_FTRACE_FTRACE_DATA_SOURCE_H_

#include <memory>
#include <vector>

#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "src/traced/probes/ftrace/cpu_reader.h"
#include "src/traced/probes/probes_data_source.h"

#include "protos/perfetto/config/ftrace/frozen_ftrace_config.gen.h"

namespace perfetto {
struct FtraceDataSourceConfig;
class FtraceProcfs;
class ProtoTranslationTable;

namespace base {
class TaskRunner;
}

// Consumes the contents of a stopped tracefs instance, converting them to
// perfetto ftrace protos (same as FtraceDataSource). Does not reactivate the
// instance or write to any other control files within the tracefs instance (but
// the buffer contents do get consumed).
class FrozenFtraceDataSource : public ProbesDataSource {
 public:
  static const ProbesDataSource::Descriptor descriptor;

  FrozenFtraceDataSource(base::TaskRunner* task_runner,
                         const DataSourceConfig& ds_config,
                         TracingSessionID session_id,
                         std::unique_ptr<TraceWriter> writer);

  ~FrozenFtraceDataSource() override;
  FrozenFtraceDataSource(const FrozenFtraceDataSource&) = delete;
  FrozenFtraceDataSource& operator=(const FrozenFtraceDataSource&) = delete;
  FrozenFtraceDataSource(FrozenFtraceDataSource&&) = delete;
  FrozenFtraceDataSource& operator=(FrozenFtraceDataSource&&) = delete;

  // ProbesDataSource implementation:
  void Start() override;
  void Flush(FlushRequestID, std::function<void()> callback) override;

 private:
  void ReadTask();

  base::TaskRunner* const task_runner_;
  std::unique_ptr<TraceWriter> writer_;

  protos::gen::FrozenFtraceConfig ds_config_;

  std::unique_ptr<FtraceProcfs> tracefs_;
  std::unique_ptr<ProtoTranslationTable> translation_table_;
  std::unique_ptr<FtraceDataSourceConfig> parsing_config_;
  CpuReader::ParsingBuffers parsing_mem_;
  std::vector<CpuReader> cpu_readers_;

  // TODO: UNIMPLEMENTED: there needs to be some kind of state / page quota
  // tracking for which per-cpu buffers have been fully read out (or are
  // supplying bad data), so that the implementation can stop reposting
  // ReadTask(). Regardless of reading errors or if the tracefs instance starts
  // recording new data, this data source should not read more than one buffer
  // capacity's worth of data from each per-cpu buffer.
  std::vector<size_t> cpu_page_quota_;

  base::WeakPtrFactory<FrozenFtraceDataSource> weak_factory_;  // Keep last.
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_FTRACE_DATA_SOURCE_H_
