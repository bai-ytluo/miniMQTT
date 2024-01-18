#include <iostream>

#include "./src/mqtt.h"
#include "./src/pack.h"

int main() {
    mqtt::Tester a, b; 
    int val_a = 0, val_b = 0; 
    std::cin>>val_a; 
    std::cin>>val_b; 
    a.set_val(val_a); b.set_val(val_b); 
    std::cout<<mqtt::testing(a,b)<<std::endl; 
    return 0; 
}
