#ifndef NACHOS_VMEM_SWAP__HH
#define NACHOS_VMEM_SWAP__HH

class OpenFile;

class SWAP {
public:
    SWAP(unsigned id);
    ~SWAP();
    void WriteSWAP(unsigned vpn, unsigned ppn);
    void PullSWAP(unsigned vpn, unsigned ppn);

private:
    char name[10];
    OpenFile *swapFile;
};

#endif
