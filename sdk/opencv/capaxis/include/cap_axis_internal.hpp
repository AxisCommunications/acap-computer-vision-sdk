// Copyright (C) 2020, Axis Communications AB, Lund

/*!
 * \file cap_axis_internal.hpp
 *
 * \brief Axis closed source code for OpenCV extracted from cap_axis_vdo
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
