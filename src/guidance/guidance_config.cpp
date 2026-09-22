#include "guidance_config.h"

#include <QtDebug>


GuidanceFlow::~GuidanceFlow() {
  clear();
}

void GuidanceFlow::addStep(GuidanceStep* step) {
  if (step == nullptr) {
    qWarning() << __FUNCTION__ << "foolish! step is nullptr!";
    return;
  }
  if (steps_.contains(step)) {
    qWarning() << __FUNCTION__ << "foolish! step is already existed!";
    return;
  }
  steps_.append(step);
}

void GuidanceFlow::clear() {
  auto it = steps_.begin();
  while (it != steps_.end()) {
    delete (*it);
    *it++ = nullptr;
  }
  steps_.clear();
}

GuidanceConfig::GuidanceConfig() = default;

GuidanceConfig::~GuidanceConfig() {
  clear();
}

GuidanceFlow* GuidanceConfig::addFlow(GuidanceFlow* flow) {
  if (flow == nullptr) {
    qWarning() << __FUNCTION__ << "foolish! flow is nullptr!";
    return flow;
  }
  if (flows_.contains(flow)) {
    qWarning() << __FUNCTION__ << "foolish! flow is already existed!";
    return flow;
  }
  flows_.append(flow);
  return flow;
}

void GuidanceConfig::clear() {
  auto it = flows_.begin();
  while (it != flows_.end()) {
    delete (*it);
    *it++ = nullptr;
  }
  flows_.clear();
}

int GuidanceConfig::totalSteps() const {
  int total = 0;
  for (auto flow : flows_) {
    if (flow != nullptr) {
      total += flow->stepCount();
    }
  }
  return total;
}

 
