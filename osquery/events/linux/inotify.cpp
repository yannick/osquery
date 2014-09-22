// Copyright 2004-present Facebook. All Rights Reserved.

#include <linux/limits.h>

#include "osquery/events.h"
#include "osquery/events/linux/inotify.h"

#include <glog/logging.h>

namespace osquery {

int kINotifyLatency = 1;
static const uint32_t BUFFER_SIZE = (10 * \
  ((sizeof (struct inotify_event)) + NAME_MAX + 1));

void INotifyEventType::setUp() {
  inotify_handle_ = ::inotify_init();
  // If this does not work throw an exception.
  if (inotify_handle_ == -1) {
    // Todo: throw exception and DO NOT register this eventtype.
  }
}

void INotifyEventType::configure() {
  // Not optimizing on watches for now.
}

void INotifyEventType::tearDown() {
  ::close(inotify_handle_);
  inotify_handle_ = -1;
}

Status INotifyEventType::run() {
  // Get a while wraper for free.
  char buffer[BUFFER_SIZE];

  fd_set set;
  struct timeval timeout;

  FD_ZERO(&set);
  FD_SET(getHandle(), &set);
  
  double sec;
  double frac = ::modf(kINotifyLatency, &sec);
  timeout.tv_sec = sec;
  timeout.tv_usec = 1000 * 1000 * frac;

  int selector = ::select(getHandle() + 1, &set, nullptr, nullptr, &timeout);
  if (selector == -1) {
    LOG(ERROR) << "Could not read inotify handle";
    return Status(1, "INotify handle failed");
  }

  if (selector == 0) {
    // Read timeout.
    return Status(0, "Continue");
  }

  ssize_t record_num = ::read(getHandle(), buffer, BUFFER_SIZE);
  LOG(INFO) << "INotify read " << record_num << " event records";
  if (record_num == 0 || record_num == -1) {
    return Status(1, "INotify read failed");
  }

  for (char *p = buffer; p < buffer + record_num;) {
    // Cast the inotify struct, make shared pointer, and append to contexts.
    auto event = reinterpret_cast<struct inotify_event*>(p);
    auto ec = createEventContext(event);
    fire(ec);
    // Continue to iterate
    p += (sizeof(struct inotify_event)) + event->len;
  }

  // Fire all inotify event contexts.
  //fire(event_contexts, 0);

  ::sleep(kINotifyLatency);
  return Status(0, "Continue");
}

INotifyEventContextRef INotifyEventType::createEventContext(
    struct inotify_event* event) {
  auto shared_event = boost::make_shared<struct inotify_event>(*event);
  auto ec = createEventContext();
  ec->event = shared_event;
  return ec;
}

Status INotifyEventType::addMonitor(const MonitorRef monitor) {
  EventType::addMonitor(monitor);
  // Instead of keeping track of every path, act greedy.
  const auto& mc = getMonitorContext(monitor->context);

  // Add the inotify watch.
  int watch = ::inotify_add_watch(getHandle(), mc->path.c_str(), IN_ALL_EVENTS);
  if (watch == -1) {
    LOG(ERROR) << "Could not add inotify watch on: " << mc->path;
    return Status(1, "Add Watch Failed");
  }

  descriptors_.push_back(watch);
  path_descriptors_[mc->path] = watch;
  descriptor_paths_[watch] = mc->path;

  return Status(0, "OK");
}

bool INotifyEventType::isMonitored(const std::string& path) {
  return (path_descriptors_.find(path) != path_descriptors_.end());
}

void INotifyEventType::processDirEvent(struct inotify_event* event) {

}

void INotifyEventType::processNodeEvent(struct inotify_event* event) {

}

void INotifyEventType::processEvent(struct inotify_event* event) {

}

}