#include "tour_catalog.h"

#include <QtDebug>


TourTopic::~TourTopic() {
  clear();
}

void TourTopic::addStep(TourStep* step) {
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

void TourTopic::clear() {
  auto it = steps_.begin();
  while (it != steps_.end()) {
    delete (*it);
    *it++ = nullptr;
  }
  steps_.clear();
}

TourCatalog::TourCatalog() = default;

TourCatalog::~TourCatalog() {
  clear();
}

TourTopic* TourCatalog::addTopic(TourTopic* topic) {
  if (topic == nullptr) {
    qWarning() << __FUNCTION__ << "foolish! topic is nullptr!";
    return topic;
  }
  if (topics_.contains(topic)) {
    qWarning() << __FUNCTION__ << "foolish! topic is already existed!";
    return topic;
  }
  topics_.append(topic);
  return topic;
}

void TourCatalog::clear() {
  auto it = topics_.begin();
  while (it != topics_.end()) {
    delete (*it);
    *it++ = nullptr;
  }
  topics_.clear();
}

int TourCatalog::totalSteps() const {
  int total = 0;
  for (auto topic : topics_) {
    if (topic != nullptr) {
      total += topic->stepCount();
    }
  }
  return total;
}

 
