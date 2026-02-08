#pragma once

struct IFdRegisterer {
    virtual void Register(int fd) = 0;
    virtual void Deregister(int fd) = 0;
};
