#ifndef NACHOS_VMEM_SWAP__HH
#define NACHOS_VMEM_SWAP__HH

class OpenFile;

class Swap {
public:
    Swap(unsigned id);
    ~Swap();
    void WriteSwap(unsigned vpn, unsigned ppn);
    void PullSwap(unsigned vpn, unsigned ppn);

private:
    char name[10];
    OpenFile *swapFile;
};

#endif
