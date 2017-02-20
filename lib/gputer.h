#ifndef WIN32
#pragma once

#include <exception>
#include <string>
#include <memory>
#include <stdint.h>

#include "exys.h"
#include "jitter.h"

namespace Exys
{

class Gputer
{
public:
    Gputer(std::unique_ptr<Jitter> graph);
    virtual ~Gputer();

    std::string GetDOTGraph();

    const std::vector<Node::Ptr>& GetInputDesc()    const { return mJitter->GetInputDesc(); }
    const std::vector<Node::Ptr>& GetObserverDesc() const { return mJitter->GetInputDesc(); }
    std::string GetPTX() const { return mPtxBuf; }

    static std::unique_ptr<Gputer> Build(const std::string& text);

private:
    void CompleteBuild();
    
    std::unique_ptr<Jitter> mJitter;
    std::string mPtxBuf;
};


};
#endif
