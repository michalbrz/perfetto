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

#include "src/traced/probes/ftrace/frozen_ftrace_data_source.h"

#include <memory>

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/flat_hash_map.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "src/traced/probes/ftrace/cpu_reader.h"
#include "src/traced/probes/ftrace/event_info.h"
#include "src/traced/probes/ftrace/event_info_constants.h"
#include "src/traced/probes/ftrace/ftrace_config_muxer.h"
#include "src/traced/probes/ftrace/ftrace_procfs.h"
#include "src/traced/probes/ftrace/proto_translation_table.h"

#include "protos/perfetto/config/data_source_config.gen.h"
#include "protos/perfetto/config/ftrace/frozen_ftrace_config.gen.h"

namespace perfetto {

// static
const FrozenFtraceDataSource::Descriptor FrozenFtraceDataSource::descriptor = {
    /* name */ "linux.frozen_ftrace",
    /* flags */ Descriptor::kFlagsNone,
    /*fill_descriptor_func*/ nullptr,
};

FrozenFtraceDataSource::~FrozenFtraceDataSource() = default;

FrozenFtraceDataSource::FrozenFtraceDataSource(
    base::TaskRunner* task_runner,
    const DataSourceConfig& ds_config,
    TracingSessionID session_id,
    std::unique_ptr<TraceWriter> writer)
    : ProbesDataSource(session_id, &descriptor),
      task_runner_(task_runner),
      writer_(std::move(writer)),
      weak_factory_(this) {
  ds_config_.ParseFromString(ds_config.frozen_ftrace_config_raw());
}

void FrozenFtraceDataSource::Start() {
  parsing_mem_.AllocateIfNeeded();

  // TODO: See "AbsolutePathForInstance" in ftrace_controller.cc for path
  // sanitization if instance name is to be supplied by the config.
  tracefs_ = FtraceProcfs::CreateGuessingMountPoint("");
  if (!tracefs_)
    return;

  // TODO: unclear on how "format" file stashing will work, so this prototype
  // doesn't implement reading from a given path.
  // NB: regardless, it cannot be an arbitrary path from the config, as the
  // format parser is not written to be durable against untrusted inputs.
  translation_table_ = ProtoTranslationTable::Create(
      tracefs_.get(), GetStaticEventInfo(), GetStaticCommonFieldsInfo());
  if (!translation_table_)
    return;

  // TODO: assumes the same core count as currently (sensible).
  size_t num_cpus = tracefs_->NumberOfCpus();
  // TODO: assumes the tracefs instance is using CLOCK_BOOTTIME (encoded as
  // unspecified since it's the implicit default). That should probably be a
  // mandatory requirement for the frozen_ftrace data source.
  const auto ftrace_clock =
      protos::pbzero::FtraceClock::FTRACE_CLOCK_UNSPECIFIED;

  cpu_readers_.reserve(num_cpus);
  for (size_t cpu = 0; cpu < num_cpus; cpu++) {
    // TODO: the two nullptrs are ok, the implementation treats those pointers
    // as nullable. We may factor them out of the contructor in the future.
    cpu_readers_.emplace_back(cpu, tracefs_->OpenPipeForCpu(cpu),
                              translation_table_.get(),
                              /*symbolizer=*/nullptr, ftrace_clock,
                              /*ftrace_clock_snapshot=*/nullptr);

    // TODO: UNIMPLEMENTED: see comment on |cpu_page_quota_|, this is just an
    // example of a page quota approach with a hardcoded limit.
    size_t initial_page_quota = 1024;
    cpu_page_quota_.push_back(initial_page_quota);
  }
  if (cpu_readers_.empty())
    return;

  // Build the struct that informs which events to convert, and how.

  // TODO: UNIMPLEMENTED: what should the filter have? All events implicitly
  // enabled?
  EventFilter event_filter;

  parsing_config_ =
      std::unique_ptr<FtraceDataSourceConfig>(new FtraceDataSourceConfig(
          /*event_filter=*/std::move(event_filter),
          /*syscall_filter=*/EventFilter{}, CompactSchedConfig{false},
          /*print_filter=*/std::nullopt,
          /*atrace_apps=*/{},
          /*atrace_categories=*/{},
          /*atrace_categories_sdk_optout=*/{},
          /*symbolize_ksyms=*/false,
          /*buffer_percent=*/0u,
          /*syscalls_returning_fd=*/{},
          /*kprobes=*/
          base::FlatHashMap<uint32_t, protos::pbzero::KprobeEvent::KprobeType>{
              0},
          /*debug_ftrace_abi=*/false,
          /*write_generic_evt_descriptors=*/false));

  // Start the reader tasks, which will self-repost until the existing raw
  // buffer pages have been parsed. The work is split into tasks to let other
  // ipc/tasks to run inbetween.
  task_runner_->PostTask([weak_this = weak_factory_.GetWeakPtr()] {
    if (weak_this)
      weak_this->ReadTask();
  });
}

void FrozenFtraceDataSource::ReadTask() {
  // TODO: ReadFrozen doesn't do internal batching (as ReadCycle would), so this
  // size must be <= parsing_mem_.ftrace_data_buf_pages. Maybe just set it to
  // equal ftrace_data_buf_pages.
  size_t max_pages = 32;

  bool all_cpus_done = true;
  for (size_t i = 0; i < cpu_readers_.size(); i++) {
    size_t pages_read = cpu_readers_[i].ReadFrozen(
        &parsing_mem_, max_pages, parsing_config_.get(), writer_.get());
    PERFETTO_DCHECK(pages_read <= max_pages);

    // TODO: UNIMPLEMENTED: handle |cpu_page_quota_| or implement an alternative
    // scheme for detecting the reads should stop.
    if (pages_read == max_pages) {
      all_cpus_done = false;
    }
  }

  // More work to do, repost the task at the end of the queue.
  if (!all_cpus_done) {
    task_runner_->PostTask([weak_this = weak_factory_.GetWeakPtr()] {
      if (weak_this)
        weak_this->ReadTask();
    });
  }
}

// Nothing to do on flush beyond making sure that the TraceWriter has committed
// its data.
void FrozenFtraceDataSource::Flush(FlushRequestID,
                                   std::function<void()> callback) {
  writer_->Flush(std::move(callback));
}

}  // namespace perfetto
