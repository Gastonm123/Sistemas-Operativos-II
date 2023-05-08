/// Entry points into the Nachos kernel from user programs.
///
/// There are two kinds of things that can cause control to transfer back to
/// here from user code:
///
/// * System calls: the user code explicitly requests to call a procedure in
///   the Nachos kernel.  Right now, the only function we support is `Halt`.
///
/// * Exceptions: the user code does something that the CPU cannot handle.
///   For instance, accessing memory that does not exist, arithmetic errors,
///   etc.
///
/// Interrupts (which can also cause control to transfer from user code into
/// the Nachos kernel) are handled elsewhere.
///
/// For now, this only handles the `Halt` system call.  Everything else core-
/// dumps.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "syscall.h"
#include "filesys/directory_entry.hh"
#include "threads/system.hh"
#include "lib/table.hh"
#include "userprog/args.hh"

#include <stdio.h>


static void
IncrementPC()
{
    unsigned pc;

    pc = machine->ReadRegister(PC_REG);
    machine->WriteRegister(PREV_PC_REG, pc);
    pc = machine->ReadRegister(NEXT_PC_REG);
    machine->WriteRegister(PC_REG, pc);
    pc += 4;
    machine->WriteRegister(NEXT_PC_REG, pc);
}

/// Do some default behavior for an unexpected exception.
///
/// NOTE: this function is meant specifically for unexpected exceptions.  If
/// you implement a new behavior for some exception, do not extend this
/// function: assign a new handler instead.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
static void
DefaultHandler(ExceptionType et)
{
    int exceptionArg = machine->ReadRegister(2);

    fprintf(stderr, "Unexpected user mode exception: %s, arg %d.\n",
            ExceptionTypeToString(et), exceptionArg);
    ASSERT(false);
}

/// Por alguna razon GCC le gusta usar las direcciones $sp y $sp+4 para guardar
/// $a0 y $a1 en vez de reservar su propio espacio. Probablemente lo mismo pase
/// con $a2 y $a3.  Asi que reduciendo en 16 el stack pointer se evita
/// sobreescribir el contenido de argv. En teoria no deberia haber otros efectos
/// secundarios.
void
FixStack()
{
    int sp = machine->ReadRegister(STACK_REG);
    machine->WriteRegister(STACK_REG, sp-16);
}

/// Run a user program.
void
RunUserProgram(void *_argv) {
    currentThread->space->InitRegisters();// Inicializa registro del user space.
    currentThread->space->RestoreState();// Carga la tabla de paginacion en la MMU.

    char** argv = (char**) _argv;

    if (argv == nullptr) {
        machine->WriteRegister(4, 0);
        machine->WriteRegister(5, 0);
    }
    else {
        machine->WriteRegister(4, WriteArgs(argv));
        int sp = machine->ReadRegister(STACK_REG);
        machine->WriteRegister(5, sp);
    }

    FixStack(); // Ad-hoc fix para algunas rarezas de GCC.

    machine->Run(); // Jump to user program.
    ASSERT(false);  // `machine->Run` never returns; the address space
                    // exits by doing the system call `Exit`.
}

const int SC_FAILURE = -1;
const int SC_SUCCESS = 0;

/// Handle a system call exception.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
///
/// The calling convention is the following:
///
/// * system call identifier in `r2`;
/// * 1st argument in `r4`;
/// * 2nd argument in `r5`;
/// * 3rd argument in `r6`;
/// * 4th argument in `r7`;
/// * the result of the system call, if any, must be put back into `r2`.
///
/// And do not forget to increment the program counter before returning. (Or
/// else you will loop making the same system call forever!)
static void
SyscallHandler(ExceptionType _et)
{
    int scid = machine->ReadRegister(2);
    Table<OpenFile*> *openFiles = currentThread->openFiles;

    switch (scid) {

        case SC_HALT:
            DEBUG('e', "Shutdown, initiated by user program.\n");
            interrupt->Halt();
            break;

        case SC_CREATE: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            DEBUG('e', "`Create` requested for file `%s`.\n", filename);

            // TODO: deberiamos recuperarlo de r5? La funcion no toma ningun argumento.
            unsigned initialSize = 0;

            // Deberia estar bien; si ya existe, simplemente se trunca.
            if (!fileSystem->Create(filename, initialSize)) {
                DEBUG('e', "Error: ocurrio un error con el sistema de archivos.\n");
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            machine->WriteRegister(2, SC_SUCCESS);
            break;
        }

        case SC_REMOVE: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            DEBUG('e', "`Remove` pedido para archivo %s.\n", filename);
            if (!fileSystem->Remove(filename)) {
                DEBUG('e', "Error: error intentando remover el archivo %s.\n", filename);
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            machine->WriteRegister(2, SC_SUCCESS);
            break;
        }
 
        case SC_EXIT: {
            int status = machine->ReadRegister(4);
            DEBUG('e', "`Exit` pedido para el proceso actual con exit code"
                  "%d.\n", status);

            // TODO: y con el argumento `status`que pasa?
            currentThread->Exit(status);
            ASSERT(false);// not reached.
            break;
        }

        case SC_OPEN : {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            DEBUG('e', "`Open` pedido para el archivo `%s`.\n", filename);

            OpenFile *file = fileSystem->Open(filename);

            if (!file) {
                DEBUG('e', "Error: ocurrio un error con el sistema de archivos.\n");
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            int fd = openFiles->Add(file);// -1 en caso de error
            machine->WriteRegister(2, fd);
            if (fd < 0) {
                DEBUG('e', "Error: numero maximo de archivos abiertos alcanzado.\n");
            }

            break;
        }

        case SC_CLOSE: {
            int fd = machine->ReadRegister(4);
            if (fd < 0) {
                DEBUG('e', "Error: file descriptor invalido.\n");
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            OpenFile *file;
            if ((file = openFiles->Remove(fd)) == nullptr) {
                DEBUG('e', "Error: file descriptor no existente.\n");
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            DEBUG('e', "`CLOSE` pedido para el file descriptor `%d`.\n", fd);

            delete file;
            machine->WriteRegister(2, SC_SUCCESS);

            break;
        }

        case SC_EXEC: {
            int filenameAddr = machine->ReadRegister(4);
            int argvAddr = machine->ReadRegister(5);

            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            OpenFile *file = fileSystem->Open(filename);
            if (!file) {
                DEBUG('e', "Error: ocurrio un error con el sistema de archivos.\n");
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }
            
            char** argv;
            
            if (argvAddr == 0) {
                argv = nullptr;
            }
            else {
                argv = SaveArgs(argvAddr);
            }

            DEBUG('e', "`EXEC` pedido para el ejecutable `%s`.\n", filename);

            AddressSpace *space = new AddressSpace(file);
            Thread *thread = new Thread("user process");
            thread->space = space;
            thread->Fork(RunUserProgram, argv);

            unsigned tid = thread->GetTid();
            machine->WriteRegister(2, tid);

            break;
        }

        case SC_JOIN: {
            int tid = machine->ReadRegister(4);
            if (tid < 0) {
                DEBUG('e', "Error: invalid space identifier.\n");
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            Thread *target = threadMap->Get(tid);
            if (!target) {
                DEBUG('e', "Error: non existent space identifier.\n");
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }

            DEBUG('e', "`JOIN` pedido sobre el thread id `%d`.\n", tid);

            /// Joinear con tid.
            int status = currentThread->Join(target);
            machine->WriteRegister(2, status);

            break;
        }

        case SC_WRITE: {
            int bufferAddr = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            OpenFileId fd = machine->ReadRegister(6);
            
            DEBUG('e', "`WRITE` pedido para el file descriptor `%d`.\n", fd);

            if (bufferAddr == 0) {
                DEBUG('e', "Error: direccion de usuario invalida.\n");
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }
            
            if (size == 0) {
                machine->WriteRegister(2, 0);
                break;
            }

            char *buffer = new char[size]; 
            ReadBufferFromUser(bufferAddr, buffer, size);
            int numbytes;

            if (fd == CONSOLE_OUTPUT) {
                ui->Write(buffer, size);
                numbytes = size;
            }
            else {
                OpenFile* file = openFiles->Get(fd);
                if (file == nullptr) {
                    DEBUG('e', "Error: file descriptor no existente.\n");
                    machine->WriteRegister(2, SC_FAILURE);
                    goto WRITE_FAILURE;
                }
                numbytes = file->Write(buffer, size);
                if (numbytes == 0) {
                    DEBUG('e', "Error: no se pudo realizar la escritura.\n");
                    machine->WriteRegister(2, SC_FAILURE);
                    goto WRITE_FAILURE;
                }
            }

            machine->WriteRegister(2, numbytes);
WRITE_FAILURE:
            delete buffer;
            break;
        } 
        
        case SC_READ: {
            int bufferAddr = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            OpenFileId fd = machine->ReadRegister(6);

            DEBUG('e', "`READ` pedido para el file descriptor `%d`.\n", fd);

            if (bufferAddr == 0) {
                DEBUG('e', "Error: direccion de usuario invalida.\n");
                machine->WriteRegister(2, SC_FAILURE);
                break;
            }
            
            if (size == 0) {
                machine->WriteRegister(2, 0);
                break;
            }

            char *buffer = new char[size]; 
            int numbytes;

            if (fd == CONSOLE_INPUT) {
                ui->Read(buffer, size);
                numbytes = size;
            }
            else {
                OpenFile* file = openFiles->Get(fd);
                if (file == nullptr) {
                    DEBUG('e', "Error: file descriptor no existente.\n");
                    machine->WriteRegister(2, SC_FAILURE);
                    goto READ_FAILURE;
                }
                numbytes = file->Read(buffer, size);
                if (numbytes == 0) {
                    DEBUG('e', "Error: no se pudo realizar la lectura.\n");
                    machine->WriteRegister(2, SC_FAILURE);
                    goto READ_FAILURE;
                }
            }

            WriteBufferToUser(buffer, bufferAddr, numbytes);
            machine->WriteRegister(2, numbytes);
READ_FAILURE:
            delete buffer;
            break;
        }
        
        case SC_PS: {
            DEBUG('e', "`PS` pedido para el proceso actual");
            scheduler->Print();
            break;
        }

        default:
            fprintf(stderr, "Unexpected system call: id %d.\n", scid);
            ASSERT(false);

    }

    IncrementPC();
}


/// By default, only system calls have their own handler.  All other
/// exception types are assigned the default handler.
void
SetExceptionHandlers()
{
    machine->SetHandler(NO_EXCEPTION,            &DefaultHandler);
    machine->SetHandler(SYSCALL_EXCEPTION,       &SyscallHandler);
    machine->SetHandler(PAGE_FAULT_EXCEPTION,    &DefaultHandler);
    machine->SetHandler(READ_ONLY_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(BUS_ERROR_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(ADDRESS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(OVERFLOW_EXCEPTION,      &DefaultHandler);
    machine->SetHandler(ILLEGAL_INSTR_EXCEPTION, &DefaultHandler);
}
