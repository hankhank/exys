#include <iostream>

#include "exys.h"

int main()
{
    const auto& d = std::string("(input double Books)\n(define res (+ 0 Books.bids.level0.price Books.asks.level1.price))\n(set! res (+ res 1))\n(output res Double.val)");
    
    auto graph = Exys::Graph::BuildGraph(d);
    return 0;
}
