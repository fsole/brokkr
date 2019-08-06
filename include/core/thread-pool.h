#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace bkk
{
  namespace core
  {
    class thread_pool_t
    {
    public:     

      class task_t
      {
      public:
        task_t();
        virtual ~task_t();

        bool hasDependenciesRemaining();
        void dependsOn(task_t* task);
        
        bool hasCompleted() { return hasCompleted_; }
        
        virtual void begin(); //Called by the threadPool when task is added to the pool
        virtual void end();   //Called by the threadPool when task has finished executing
        virtual void run() = 0;

      private:

        bool addDependency(task_t* dependentTask);
        void clearOneDependency();

        std::vector<task_t*> dependentTask_;  //Tasks that depends on this task        
        std::atomic<int>     dependenciesRemaining_;  //Number of task that need to finish for this task to be ready
        bool                 hasCompleted_; //Task has executed
      };

      thread_pool_t(unsigned int numThreads);
      ~thread_pool_t();

      void addTask(task_t* task);
      void exit();
      void waitForCompletion();

    private:

      struct worker_thread_t
      {
        worker_thread_t(thread_pool_t* pool);
        ~worker_thread_t();

        static void* run(void* data);

        thread_pool_t* pool_;    //Pointer to thread pool
        std::thread    thread_;  //Thread
        bool           exit_;
      };

      task_t* getNextTask();
      void endTask(task_t* task);

      std::vector<worker_thread_t*>  workerThread_;  //worker threads
      std::vector<task_t*>           taskReady_;     //list of tasks ready to run
      std::vector<task_t*>           taskNotReady_;  //list of tasks not ready to run (depends on another task that has not been executed yet)
      std::mutex                     mutex_;
      std::condition_variable        conditionVar_;
      std::atomic<int>               pendingTasks_;
      std::atomic<bool>              exit_;
    };

  }
}