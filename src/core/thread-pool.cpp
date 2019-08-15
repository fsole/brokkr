#include "core/thread-pool.h"

using namespace bkk::core;

thread_pool_t::worker_thread_t::worker_thread_t(thread_pool_t* pool)
:exit_(false),
 pool_(pool)
{
  thread_ = std::thread(&worker_thread_t::run, this);
}

thread_pool_t::worker_thread_t::~worker_thread_t()
{
}

void* thread_pool_t::worker_thread_t::run(void* context)
{
  worker_thread_t* workerThread = (worker_thread_t*)context;
  thread_pool_t* pool = workerThread->pool_;

  while (!workerThread->exit_)
  {
    task_t* task = pool->getNextTask();
    if (task)
    {      
      //Execute task
      task->run();
      pool->endTask(task);
    }
  }

  return 0;
}

thread_pool_t::thread_pool_t(unsigned int numThreads)
 :pendingTasks_(0),
  exit_(false)
{  
  workerThread_.resize(numThreads);
  for (unsigned int i(0); i < workerThread_.size(); ++i)
    workerThread_[i] = new worker_thread_t(this);  
}

thread_pool_t::~thread_pool_t()
{
}

thread_pool_t::task_t* thread_pool_t::getNextTask()
{
  std::unique_lock<std::mutex> lock(mutex_);

  auto it = taskNotReady_.begin();
  while( it != taskNotReady_.end() )
  {
    if ((*it)->hasDependenciesRemaining() == false)
    {
      taskReady_.push_back(*it);
      it = taskNotReady_.erase(it);
      conditionVar_.notify_one();
    }
    else
    {
      it++;
    }
  }

  while (taskReady_.empty() && !exit_)
  {
    conditionVar_.wait(lock);
  }

  if (exit_ || taskReady_.empty())
  {
    return nullptr;
  }
  else
  {
    task_t* task = taskReady_[0];
    taskReady_.erase(taskReady_.begin());
    return task;
  }
}

void thread_pool_t::endTask(task_t* task)
{
  task->end();
  pendingTasks_--;
}

void thread_pool_t::addTask(task_t* task)
{
  task->begin();
  std::unique_lock<std::mutex> lock(mutex_);
  pendingTasks_++;
  if (task->hasDependenciesRemaining())
  {
    taskNotReady_.push_back(task);
  }
  else
  {
    taskReady_.push_back(task);
    conditionVar_.notify_one();
  }
}

void thread_pool_t::exit()
{
  for (unsigned int i(0); i < workerThread_.size(); ++i)
  {
    workerThread_[i]->exit_ = true;
  }

  exit_ = true;  
  conditionVar_.notify_all();

  //Wait for all thread to finish and destroy them
  for (unsigned int i(0); i < workerThread_.size(); ++i)
    workerThread_[i]->thread_.join();
    
  for (unsigned int i(0); i < workerThread_.size(); ++i)
    delete workerThread_[i];
}

void thread_pool_t::waitForCompletion()
{
  while (pendingTasks_ != 0)
  {
  }
}


thread_pool_t::task_t::task_t()
:dependenciesRemaining_(0),
 hasCompleted_(false)
{
}

thread_pool_t::task_t::~task_t()
{
}

bool thread_pool_t::task_t::hasDependenciesRemaining()
{
  return dependenciesRemaining_ > 0;
}

void thread_pool_t::task_t::dependsOn(thread_pool_t::task_t* task)
{
  if( task->addDependency(this) )
    dependenciesRemaining_++;
}

bool thread_pool_t::task_t::addDependency(thread_pool_t::task_t* dependentTask)
{
  for (uint32_t i(0); i < dependentTask_.size(); ++i)
  {
    //Dependency is already in the list
    if (dependentTask_[i] == dependentTask)
      return false;
  }

  dependentTask_.push_back(dependentTask);
  return true;
}

void thread_pool_t::task_t::clearOneDependency()
{
  dependenciesRemaining_--;
}

void thread_pool_t::task_t::begin()
{ 
  hasCompleted_ = false; 
}

void thread_pool_t::task_t::end()
{
  //Signal dependent tasks
  size_t count(dependentTask_.size());
  for (size_t i(0); i < count; ++i)
  {
    dependentTask_[i]->clearOneDependency();
  }

  hasCompleted_ = true;
}

uint32_t bkk::core::getCPUCoreCount() 
{ 
  return std::thread::hardware_concurrency(); 
}