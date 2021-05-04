/**
* Copyright (C) 2020 Axis Communications AB, Lund, Sweden
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/


#ifndef CAP_AXIS_INTERNAL_HPP
#define CAP_AXIS_INTERNAL_HPP

class AxisInternal {
public:
    static AxisInternal* getInstance();
    bool SetProperty(int property, double value);
    double GetProperty(int property, timespec grabbed_ts);

private:
    static AxisInternal* inst_;
    AxisInternal();
    AxisInternal(const AxisInternal&);
    AxisInternal& operator=(const AxisInternal&);
};

#endif // CAP_AXIS_INTERNAL_HPP
