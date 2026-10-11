#include "support.hpp"
#include "production.inc"

int assertions = 0;
void check(bool condition, const std::string &message) {
  ++assertions;
  if (!condition) throw std::runtime_error(message);
}
QList<int> sorted(QList<int> values) {
  std::sort(values.begin(), values.end());
  return values;
}
void load(int id, bool valid = true) {
  Configs::manager.loaded[id] = std::make_shared<Configs::ProxyEntity>(
      Configs::ProxyEntity{id, 1, 10, valid});
}
void runUpdateWorker() {
  check(Fixture::updateQueue.size() == 1, "one asynchronous update worker is queued");
  auto job = std::move(Fixture::updateQueue.front());
  Fixture::updateQueue.pop_front();
  std::exception_ptr error;
  std::thread worker([&] { try { job(); } catch (...) { error = std::current_exception(); } });
  worker.join();
  if (error) std::rethrow_exception(error);
}
void deliverGui() {
  check(Fixture::guiQueue.size() == 1, "exactly one GUI completion is queued");
  auto job = std::move(Fixture::guiQueue.front());
  Fixture::guiQueue.pop_front();
  job();
}
void cycle(MainWindow &window, const QList<int> &expectedValidated,
           const QList<int> &expectedDeleted, bool logInvalid, bool urlTest) {
  const auto priorSignals = Fixture::signalCount;
  Fixture::validated.clear(); Fixture::deleted.clear(); Fixture::events.clear();
  Fixture::logs.clear(); Fixture::scheduled = Fixture::started = Fixture::returned = 0;
  Fixture::urlCount = 0;
  window.on_menu_update_subscription_triggered();
  check(mw_sub_updating, "menu guard is set before worker starts");
  check(Fixture::signalCount == priorSignals, "no synchronous completion signal");
  window.on_menu_update_subscription_triggered();
  runUpdateWorker();
  check(Fixture::sawBackgroundCallback, "callback stays on the asynchronous update worker");
  check(Fixture::scheduled == expectedValidated.size(), "worker count equals loaded profile count");
  check(Fixture::started == Fixture::scheduled && Fixture::returned == Fixture::scheduled,
        "every scheduled validation worker starts and returns exactly once");
  check(sorted(Fixture::validated) == sorted(expectedValidated), "expected profiles validated exactly once per loaded entry");
  check(sorted(Fixture::deleted) == sorted(expectedDeleted), "only invalid loaded entries are deleted");
  check(Fixture::validationQueue.empty(), "no validation callback outlives the stack state");
  check(QThreadPool::allocations.empty(), "callback releases its local pool before returning");
  check(Fixture::signalCount == priorSignals + 1, "AsyncUpdate emits one completion signal");
  check(Fixture::urlCount == (urlTest ? 1 : 0), "later URL-test stage retains its setting");
  std::vector<std::string> expectedEvents{"callback_enter"};
  if (urlTest) expectedEvents.emplace_back("url_test");
  expectedEvents.insert(expectedEvents.end(), {"callback_return", "completion_signal", "gui_post"});
  check(Fixture::events == expectedEvents, "post-processing returns before signal and finish callback");
  const QString expectedLog = QString("\nDeleted %1 Invalid").arg(expectedDeleted.size());
  const bool foundLog = std::any_of(Fixture::logs.begin(), Fixture::logs.end(),
      [&](const QString &log) { return log.find(expectedLog) != std::string::npos; });
  check(foundLog == logInvalid, "invalid deletion count log is preserved");
  check(mw_sub_updating, "menu guard stays set before GUI delivery");
  window.on_menu_update_subscription_triggered();
  check(Fixture::updateQueue.empty(), "request remains blocked while GUI completion is pending");
  deliverGui();
  check(!mw_sub_updating, "GUI completion releases the menu guard");
  QThreadPool::cleanup();
}

int main(int argc, char **argv) {
  if (argc != 3) return 2;
  const std::string name = argv[1];
  Fixture::schedule = argv[2];
  try {
    MainWindow window;
    window.installPostUpdateJob();
    auto &ids = Configs::manager.current->profiles;
    QList<int> expectedValidated, expectedDeleted;
    bool logInvalid = true, urlTest = false;
    if (name == "scheduling_failure") {
      ids = {1, 2}; load(1, false); load(2);
      Fixture::throwAfterEnqueues = 1;
      window.on_menu_update_subscription_triggered();
      bool sawFailure = false;
      try { runUpdateWorker(); }
      catch (const Fixture::SchedulingFailure &) { sawFailure = true; }
      check(sawFailure, "scheduling error is not silently swallowed");
      check(Fixture::scheduled == 1 && Fixture::returned == 1,
            "already scheduled worker is joined while captured state is alive");
      check(Fixture::validationQueue.empty() && QThreadPool::allocations.empty(),
            "no worker or pool survives scheduling-side unwinding");
      check(Fixture::signalCount == 0 && Fixture::guiQueue.empty() && Fixture::deleted.empty(),
            "failure does not invent success, deletion, or completion policy");
      std::cout << "PASS " << name << " " << Fixture::schedule << " assertions=" << assertions << '\n';
      return 0;
    } else if (name == "empty" || name == "repeat_update") {
    } else if (name == "all_missing") {
      ids = {1, 2, 3};
    } else if (name == "repeated_missing") {
      ids = {1, 1, 1};
    } else if (name == "single_valid") {
      ids = {1}; load(1); expectedValidated = {1};
    } else if (name == "single_invalid") {
      ids = {1}; load(1, false); expectedValidated = expectedDeleted = {1};
    } else if (name == "mixed" || name == "mixed_url_test") {
      ids = {1, 2, 3, 4}; load(1); load(3, false);
      expectedValidated = {1, 3}; expectedDeleted = {3};
      urlTest = name == "mixed_url_test";
    } else if (name == "all_invalid") {
      ids = {1, 2, 3};
      for (int id : ids) load(id, false);
      expectedValidated = expectedDeleted = ids;
    } else if (name == "repeated_loaded_id") {
      ids = {1, 1, 1}; load(1); expectedValidated = ids;
    } else if (name == "empty_url_test") {
      urlTest = true;
    } else if (name == "disabled") {
      Configs::store.sub_rm_invalid = false; logInvalid = false;
      ids = {1}; load(1, false); urlTest = true;
    } else if (name == "limit_3000" || name == "over_limit") {
      const int count = name == "limit_3000" ? 3000 : 3001;
      for (int id = 1; id <= count; ++id) { ids += id; load(id); }
      if (count == 3000) expectedValidated = ids; else logInvalid = false;
    } else if (name == "duplicate_stage") {
      Configs::store.sub_rm_duplicates = true;
    } else throw std::runtime_error("unknown case");
    Configs::store.sub_url_test = urlTest;
    cycle(window, expectedValidated, expectedDeleted, logInvalid, urlTest);
    if (name == "all_missing" || name == "repeated_missing") {
      check(ids.size() == 3, "unloadable IDs are not deleted by invalid filtering");
    }
    if (name == "duplicate_stage") {
      check(!Fixture::logs.empty() && Fixture::logs.back().find("Deleted 0 Duplicates") != std::string::npos,
            "previous duplicate-stage log survives empty invalid stage");
    }
    if (name == "repeat_update") {
      ids = {1, 2}; load(1); load(2, false);
      cycle(window, {1, 2}, {2}, true, false);
      Configs::manager.loaded.clear(); ids = {9, 10};
      cycle(window, {}, {}, true, false);
    }
    std::cout << "PASS " << name << " " << Fixture::schedule << " assertions=" << assertions << '\n';
    return 0;
  } catch (const Fixture::BlockedWait &error) {
    std::cout << "FAIL " << name << " " << Fixture::schedule << ": " << error.what()
              << "; workers=" << Fixture::scheduled << " completions=" << Fixture::signalCount
              << " gui_callbacks=" << Fixture::guiQueue.size() << '\n';
  } catch (const std::exception &error) {
    std::cout << "FAIL " << name << " " << Fixture::schedule << ": " << error.what() << '\n';
  }
  // A failed run can leave callbacks capturing unwound stack state. Discard them
  // without execution, then release only the synthetic pool objects.
  Fixture::validationQueue.clear();
  QThreadPool::cleanup();
  return 1;
}
