/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */
#include "Timer.h"
#include <iostream>

using qpid::sys::AbsTime;
using qpid::sys::Duration;
using qpid::sys::Monitor;
using qpid::sys::Thread;
using namespace qpid::broker;

TimerTask::TimerTask(Duration timeout) : time(AbsTime::now(), timeout), cancelled(false) {}
TimerTask::TimerTask(AbsTime _time) : time(_time), cancelled(false) {}
TimerTask::~TimerTask(){}

Timer::Timer() : active(false) 
{
    start();
}

Timer::~Timer() 
{
    stop();
}

void Timer::run()
{
    Monitor::ScopedLock l(monitor);
    while(active){
        if (tasks.empty()) {
            monitor.wait();
        } else {
            TimerTask::shared_ptr t = tasks.top();
            if (t->cancelled) {
                tasks.pop();
            } else if(t->time < AbsTime::now()) {
                tasks.pop();
                t->fire();
            } else {
                monitor.wait(t->time);
            }
        }
    }
}

void Timer::add(TimerTask::shared_ptr task)
{
    Monitor::ScopedLock l(monitor);
    tasks.push(task);
    monitor.notify();
}

void Timer::start()
{
    Monitor::ScopedLock l(monitor);
    if (!active) {
        active = true;
        runner = std::auto_ptr<Thread>(new Thread(this));
    }
}

void Timer::stop()
{
    signalStop();
    if (runner.get()) {
        runner->join();
        runner.reset();
    }
}
void Timer::signalStop()
{
    Monitor::ScopedLock l(monitor);
    if (active) {
        active = false;
        monitor.notifyAll();
    }
}

bool Later::operator()(const TimerTask::shared_ptr& a, const TimerTask::shared_ptr& b) const
{
    return a.get() && b.get() && a->time > b->time;
}
