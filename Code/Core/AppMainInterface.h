#ifndef IAPPMAIN_H
#define IAPPMAIN_H

namespace CC
{
    class IAppMain
    {
    public:
        virtual void Init() = 0;
        virtual void Update() = 0;
        virtual void Shutdown() = 0;
    };
}
#endif // IAPPMAIN_H
