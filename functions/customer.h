#ifndef CUSTOMER_FUNCTIONS
#define CUSTOMER_FUNCTIONS

#include <sys/ipc.h>
#include <sys/sem.h>
#include "./common.h"
#include "../record-struct/feedback.h"

struct Customer loggedInCustomer;
int semIdentifier;

// Function Prototypes =================================

bool transfer_funds(int connFD);
bool customer_add_feedback(int connFD , char *custname ,int accountNumber);
bool customer_operation_handler(int connFD);
bool deposit(int connFD);
bool withdraw(int connFD);
bool get_balance(int connFD);
bool change_password(int connFD);
bool lock_critical_section(struct sembuf *semOp);
bool unlock_critical_section(struct sembuf *sem_op);
void write_transaction_to_array(int *transactionArray, int ID);
int write_transaction_to_file(int accountNumber, long int oldBalance, long int newBalance, bool operation);
bool customer_apply_for_loan(int connFD);

// =====================================================

// Function Definition =================================

bool customer_operation_handler(int connFD)
{
    if (login_handler(false, connFD, &loggedInCustomer))
    {
        ssize_t writeBytes, readBytes;            // Number of bytes read from / written to the client
        char readBuffer[1000], writeBuffer[1000]; // A buffer used for reading & writing to the client

        // Get a semaphore for the user
        key_t semKey = ftok(CUSTOMER_FILE, loggedInCustomer.account); // Generate a key based on the account number hence, different customers will have different semaphores

        union semun
        {
            int val; // Value of the semaphore
        } semSet;

        int semctlStatus;
        semIdentifier = semget(semKey, 1, 0); // Get the semaphore if it exists
        if (semIdentifier == -1)
        {
            semIdentifier = semget(semKey, 1, IPC_CREAT | 0700); // Create a new semaphore
            if (semIdentifier == -1)
            {
                perror("Error while creating semaphore!");
                _exit(1);
            }

            semSet.val = 1; // Set a binary semaphore
            semctlStatus = semctl(semIdentifier, 0, SETVAL, semSet);
            if (semctlStatus == -1)
            {
                perror("Error while initializing a binary sempahore!");
                _exit(1);
            }
        }

        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "Welcome beloved customer!");
        while (1)
        {
            strcat(writeBuffer, "\n");
            strcat(writeBuffer, "1. View Account Balance \n2. Deposit Money \n3. Withdraw Money \n4. Transfer Funds \n5. Apply for a Loan \n6. Change Password \n7. Adding Feedback \n8. View Transaction History \nPress any other key to logout");
            writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
            if (writeBytes == -1)
            {
                perror("Error while writing CUSTOMER_MENU to client!");
                return false;
            }
            bzero(writeBuffer, sizeof(writeBuffer));

            bzero(readBuffer, sizeof(readBuffer));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer));
            if (readBytes == -1)
            {
                perror("Error while reading client's choice for CUSTOMER_MENU");
                return false;
            }
            
            int choice = atoi(readBuffer);
            switch (choice)
            {
                case 1:
                    get_balance(connFD);
                    break; 
                case 2:
                    deposit(connFD);
                    break;
                case 3:
                    withdraw(connFD);
                    break;
                case 4:
                    transfer_funds(connFD);
                    break;
                case 5:
                    customer_apply_for_loan(connFD);
                    break;
                case 6:
                    change_password(connFD);
                    break; 
                case 7:
                    customer_add_feedback(connFD, loggedInCustomer.name, loggedInCustomer.account);
                    break;
                case 8:
                    get_transaction_details(connFD, loggedInCustomer.account);
                    break;      
            
            default:
                writeBytes = write(connFD, "Logging you out now dear customer !$", strlen("Logging you out now dear customer !$"));
                return false;
            }
        }
    }
    else
    {
        // CUSTOMER LOGIN FAILED
        return false;
    }
    return true;
}

bool transfer_funds(int connFD)
{
    char readBuffer[1000], writeBuffer[1000];
    ssize_t readBytes, writeBytes;
    struct Account senderAccount,receiverAccount;
    senderAccount.accountNumber = loggedInCustomer.account;

    writeBytes = write(connFD, "Enter the account number of the Receiver", strlen("Enter the account number of the Receiver"));
    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    receiverAccount.accountNumber = atoi(readBuffer);

    writeBytes = write(connFD, "How much money do you want to add in receiver's account?", strlen("How much money do you want to add in receiver's account?"));
    if (writeBytes == -1)
    {
        perror("Error writing WITHDRAW_AMOUNT message to client!");
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1)
    {
        perror("Error reading withdraw amount from client!");
        return false;
    }

    int withdrawAmount = atol(readBuffer);

    // Lock the critical section
    struct sembuf semOp;
    lock_critical_section(&semOp);

    if(get_account_details(connFD, &receiverAccount) && get_account_details(connFD, &senderAccount))
    {
        if (receiverAccount.active)
        {
            if (withdrawAmount != 0 && senderAccount.balance - withdrawAmount >= 0)
            {
                int newTransactionID = write_transaction_to_file(senderAccount.accountNumber, senderAccount.balance, senderAccount.balance - withdrawAmount, 0);
                write_transaction_to_array(senderAccount.transactions, newTransactionID);
                senderAccount.balance -= withdrawAmount;

                newTransactionID = write_transaction_to_file(receiverAccount.accountNumber, receiverAccount.balance, receiverAccount.balance + withdrawAmount, 1);
                write_transaction_to_array(receiverAccount.transactions, newTransactionID);
                receiverAccount.balance += withdrawAmount;


                //withdrawal from sender's account
                int accountFileDescriptor = open(ACCOUNT_FILE, O_WRONLY);
                off_t offset = lseek(accountFileDescriptor, senderAccount.accountNumber * sizeof(struct Account), SEEK_SET);

                struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
                int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
                if (lockingStatus == -1)
                {
                    perror("Error obtaining write lock on account record!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                writeBytes = write(accountFileDescriptor, &senderAccount, sizeof(struct Account));
                if (writeBytes == -1)
                {
                    perror("Error writing updated balance into account file!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                lock.l_type = F_UNLCK;
                fcntl(accountFileDescriptor, F_SETLK, &lock);
                close(accountFileDescriptor);


                //depositing in receiver's account
                accountFileDescriptor = open(ACCOUNT_FILE, O_WRONLY);
                offset = lseek(accountFileDescriptor, receiverAccount.accountNumber * sizeof(struct Account), SEEK_SET);

                struct flock lock2 = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
                lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock2);
                if (lockingStatus == -1)
                {
                    perror("Error obtaining write lock on account record!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                writeBytes = write(accountFileDescriptor, &receiverAccount, sizeof(struct Account));
                if (writeBytes == -1)
                {
                    perror("Error writing updated balance into account file!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                lock2.l_type = F_UNLCK;
                fcntl(accountFileDescriptor, F_SETLK, &lock2);

                write(connFD, "The amount is successfully withdrawn from your account and credited into recevier's account!^", strlen("The amount is successfully withdrawn from your account and credited into recevier's account!^"));
                read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

                get_balance(connFD);

                unlock_critical_section(&semOp);

                return true;
            }
            else
            {
                //not enough money
                writeBytes = write(connFD, "Either you have input an invalid amount or you don't have enough money to withdraw^", strlen("Either you have input an invalid amount or you don't have enough money to withdraw^"));
            }
        }
        else
        {
            //receiver account not active
            write(connFD, "It seems your account has been deactivated!^", strlen("It seems your account has been deactivated!^"));
        }
        read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        unlock_critical_section(&semOp);
        return false;
    }
    else
    {
        // FAIL  ...no receiver account found
        unlock_critical_section(&semOp);
        return false;
    }
}

bool customer_apply_for_loan(int connFD) 
{
    char readBuffer[1000], writeBuffer[1000];
    struct Loan loan;
    ssize_t readBytes, writeBytes;

    loan.customerID = loggedInCustomer.id;

    int loanFileDescriptor = open(LOAN_FILE, O_RDONLY);
    if (loanFileDescriptor == -1 && errno == ENOENT)
    {
        // Loan file was never created
        loan.loanID = 0;
    }
    else
    {
        off_t offset = lseek(loanFileDescriptor, -sizeof(struct Loan), SEEK_END);
        //applying lock
        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Loan), getpid()};
        int lockingStatus = fcntl(loanFileDescriptor, F_SETLKW, &lock);

        struct Loan prevLoan;
        readBytes = read(loanFileDescriptor, &prevLoan, sizeof(struct Loan));
        //unlocking
        lock.l_type = F_UNLCK;
        fcntl(loanFileDescriptor, F_SETLK, &lock);
        close(loanFileDescriptor);

        loan.loanID = prevLoan.loanID + 1;
    }

    sprintf(writeBuffer, "%s", "Enter the Amount of Loan\n");

    writeBytes = write(connFD, writeBuffer, sizeof(writeBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    loan.amount=atol(readBuffer);

    loan.approved=0;                // 0 -> Pending
    loan.EmployeeID=-1;             // -1 -> No employee assigned

    loanFileDescriptor = open(LOAN_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);
    writeBytes = write(loanFileDescriptor, &loan, sizeof(struct Loan));
    close(loanFileDescriptor);

    bzero(writeBuffer, sizeof(writeBuffer));
    bzero(readBuffer, sizeof(readBuffer));
    sprintf(writeBuffer, "%s%d", "The newly created loan number is :", loan.loanID);
    strcat(writeBuffer, "\nRedirecting you to the main menu ...^");
    writeBytes = write(connFD, writeBuffer, sizeof(writeBuffer));
    readBytes = read(connFD, readBuffer, sizeof(read)); // Dummy read
    return true;
}


bool customer_add_feedback(int connFD , char *custname ,int accountNumber)
{
    char readBuffer[1000], writeBuffer[1000];
    ssize_t readBytes, writeBytes;

    struct Feedback feedback;
    feedback.account = accountNumber;
    strcpy(feedback.name,custname);

    int feedbackFileDescriptor = open(FEEDBACK_FILE, O_CREAT | O_APPEND | O_RDWR, S_IRWXU);

    // Get most recent feedback number
    off_t offset = lseek(feedbackFileDescriptor, -sizeof(struct Feedback), SEEK_END);
    if (offset >= 0)
    {
        // There exists at least one feedback
        struct Feedback prevFeedback;
        readBytes = read(feedbackFileDescriptor, &prevFeedback, sizeof(struct Feedback));
        feedback.feedbackID = prevFeedback.feedbackID + 1;
    }
    else
        // No feedback exist
        feedback.feedbackID = 0;


    sprintf(writeBuffer, "%s", "Your Feedback matters for us. Please provide us your valuable feedback\n");

    writeBytes = write(connFD, writeBuffer, sizeof(writeBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    strcpy(feedback.message, readBuffer);

    writeBytes = write(feedbackFileDescriptor, &feedback, sizeof(struct Feedback));
    close(feedbackFileDescriptor);

    bzero(writeBuffer, sizeof(writeBuffer));
    bzero(readBuffer, sizeof(readBuffer));
    sprintf(writeBuffer, "%s", "Thank you for your valuable Feedback!");
    strcat(writeBuffer, "\nRedirecting you to the main menu ...^");
    writeBytes = write(connFD, writeBuffer, sizeof(writeBuffer));
    readBytes = read(connFD, readBuffer, sizeof(read)); // Dummy read
    return true;
}


bool deposit(int connFD)
{
    char readBuffer[1000], writeBuffer[1000];
    ssize_t readBytes, writeBytes;

    struct Account account;
    account.accountNumber = loggedInCustomer.account;

    long int depositAmount = 0;

    // Lock the critical section
    struct sembuf semOp;
    lock_critical_section(&semOp);

    if (get_account_details(connFD, &account))
    {
        
        if (account.active)
        {

            writeBytes = write(connFD, "How much is it that you want to add into your bank?", strlen("How much is it that you want to add into your bank?"));
            if (writeBytes == -1)
            {
                perror("Error writing DEPOSIT_AMOUNT to client!");
                unlock_critical_section(&semOp);
                return false;
            }

            bzero(readBuffer, sizeof(readBuffer));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer));
            if (readBytes == -1)
            {
                perror("Error reading deposit money from client!");
                unlock_critical_section(&semOp);
                return false;
            }

            depositAmount = atol(readBuffer);
            if (depositAmount != 0)
            {

                int newTransactionID = write_transaction_to_file(account.accountNumber, account.balance, account.balance + depositAmount, 1);
                write_transaction_to_array(account.transactions, newTransactionID);

                account.balance += depositAmount;

                int accountFileDescriptor = open(ACCOUNT_FILE, O_WRONLY);
                off_t offset = lseek(accountFileDescriptor, account.accountNumber * sizeof(struct Account), SEEK_SET);

                struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
                int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
                if (lockingStatus == -1)
                {
                    perror("Error obtaining write lock on account file!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                writeBytes = write(accountFileDescriptor, &account, sizeof(struct Account));
                if (writeBytes == -1)
                {
                    perror("Error storing updated deposit money in account record!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                lock.l_type = F_UNLCK;
                fcntl(accountFileDescriptor, F_SETLK, &lock);

                write(connFD, "The specified amount has been successfully added to your bank account!^", strlen("The specified amount has been successfully added to your bank account!^"));
                read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

                get_balance(connFD);

                unlock_critical_section(&semOp);

                return true;
            }
            else
                writeBytes = write(connFD, "You seem to have passed an invalid amount!^", strlen("You seem to have passed an invalid amount!^"));
        }
        else
            write(connFD, "It seems your account has been deactivated!^", strlen("It seems your account has been deactivated!^"));
        read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

        unlock_critical_section(&semOp);
    }
    else
    {
        // FAIL
        unlock_critical_section(&semOp);
        return false;
    }
}

bool withdraw(int connFD)
{
    char readBuffer[1000], writeBuffer[1000];
    ssize_t readBytes, writeBytes;

    struct Account account;
    account.accountNumber = loggedInCustomer.account;

    long int withdrawAmount = 0;

    // Lock the critical section
    struct sembuf semOp;
    lock_critical_section(&semOp);

    if (get_account_details(connFD, &account))
    {
        if (account.active)
        {

            writeBytes = write(connFD, "How much is it that you want to withdraw from your bank?", strlen("How much is it that you want to withdraw from your bank?"));
            if (writeBytes == -1)
            {
                perror("Error writing WITHDRAW_AMOUNT message to client!");
                unlock_critical_section(&semOp);
                return false;
            }

            bzero(readBuffer, sizeof(readBuffer));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer));
            if (readBytes == -1)
            {
                perror("Error reading withdraw amount from client!");
                unlock_critical_section(&semOp);
                return false;
            }

            withdrawAmount = atol(readBuffer);

            if (withdrawAmount != 0 && account.balance - withdrawAmount >= 0)
            {

                int newTransactionID = write_transaction_to_file(account.accountNumber, account.balance, account.balance - withdrawAmount, 0);
                write_transaction_to_array(account.transactions, newTransactionID);

                account.balance -= withdrawAmount;

                int accountFileDescriptor = open(ACCOUNT_FILE, O_WRONLY);
                off_t offset = lseek(accountFileDescriptor, account.accountNumber * sizeof(struct Account), SEEK_SET);

                struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
                int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
                if (lockingStatus == -1)
                {
                    perror("Error obtaining write lock on account record!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                writeBytes = write(accountFileDescriptor, &account, sizeof(struct Account));
                if (writeBytes == -1)
                {
                    perror("Error writing updated balance into account file!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                lock.l_type = F_UNLCK;
                fcntl(accountFileDescriptor, F_SETLK, &lock);

                write(connFD, "The specified amount has been successfully withdrawn from your bank account!^", strlen("The specified amount has been successfully withdrawn from your bank account!^"));
                read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

                get_balance(connFD);

                unlock_critical_section(&semOp);

                return true;
            }
            else
                writeBytes = write(connFD, "You seem to have either passed an invalid amount or you don't have enough money in your bank to withdraw the specified amount^", strlen("You seem to have either passed an invalid amount or you don't have enough money in your bank to withdraw the specified amount^"));
        }
        else
            write(connFD, "It seems your account has been deactivated!^", strlen("It seems your account has been deactivated!^"));
        read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

        unlock_critical_section(&semOp);
    }
    else
    {
        // FAILURE while getting account information
        unlock_critical_section(&semOp);
        return false;
    }
}

bool get_balance(int connFD)
{
    char buffer[1000];
    struct Account account;
    account.accountNumber = loggedInCustomer.account;
    if (get_account_details(connFD, &account))
    {
        bzero(buffer, sizeof(buffer));
        if (account.active)
        {
            sprintf(buffer, "You have ₹ %ld imaginary money in our bank!^", account.balance);
            write(connFD, buffer, strlen(buffer));
        }
        else
            write(connFD, "It seems your account has been deactivated!^", strlen("It seems your account has been deactivated!^"));
        read(connFD, buffer, sizeof(buffer)); // Dummy read
    }
    else
    {
        // ERROR while getting balance
        return false;
    }
}

bool change_password(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000], hashedPassword[1000];

    char newPassword[1000];

    // Lock the critical section
    struct sembuf semOp = {0, -1, SEM_UNDO};
    int semopStatus = semop(semIdentifier, &semOp, 1);
    if (semopStatus == -1)
    {
        perror("Error while locking critical section");
        return false;
    }

    writeBytes = write(connFD, "Enter your old password", strlen("Enter your old password"));
    if (writeBytes == -1)
    {
        perror("Error writing PASSWORD_CHANGE_OLD_PASS message to client!");
        unlock_critical_section(&semOp);
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1)
    {
        perror("Error reading old password response from client");
        unlock_critical_section(&semOp);
        return false;
    }

    if (strcmp(crypt(readBuffer, SALT_BAE), loggedInCustomer.password) == 0)
    {
        // Password matches with old password
        writeBytes = write(connFD, "Enter the new password", strlen("Enter the new password"));
        if (writeBytes == -1)
        {
            perror("Error writing PASSWORD_CHANGE_NEW_PASS message to client!");
            unlock_critical_section(&semOp);
            return false;
        }
        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        if (readBytes == -1)
        {
            perror("Error reading new password response from client");
            unlock_critical_section(&semOp);
            return false;
        }

        strcpy(newPassword, crypt(readBuffer, SALT_BAE));

        writeBytes = write(connFD, "Reenter the new password", strlen("Reenter the new password"));
        if (writeBytes == -1)
        {
            perror("Error writing PASSWORD_CHANGE_NEW_PASS_RE message to client!");
            unlock_critical_section(&semOp);
            return false;
        }
        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        if (readBytes == -1)
        {
            perror("Error reading new password reenter response from client");
            unlock_critical_section(&semOp);
            return false;
        }

        if (strcmp(crypt(readBuffer, SALT_BAE), newPassword) == 0)
        {
            // New & reentered passwords match

            strcpy(loggedInCustomer.password, newPassword);

            int customerFileDescriptor = open(CUSTOMER_FILE, O_WRONLY);
            if (customerFileDescriptor == -1)
            {
                perror("Error opening customer file!");
                unlock_critical_section(&semOp);
                return false;
            }

            off_t offset = lseek(customerFileDescriptor, loggedInCustomer.id * sizeof(struct Customer), SEEK_SET);
            if (offset == -1)
            {
                perror("Error seeking to the customer record!");
                unlock_critical_section(&semOp);
                return false;
            }

            struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};
            int lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
            if (lockingStatus == -1)
            {
                perror("Error obtaining write lock on customer record!");
                unlock_critical_section(&semOp);
                return false;
            }

            writeBytes = write(customerFileDescriptor, &loggedInCustomer, sizeof(struct Customer));
            if (writeBytes == -1)
            {
                perror("Error storing updated customer password into customer record!");
                unlock_critical_section(&semOp);
                return false;
            }

            lock.l_type = F_UNLCK;
            lockingStatus = fcntl(customerFileDescriptor, F_SETLK, &lock);

            close(customerFileDescriptor);

            writeBytes = write(connFD, "Password successfully changed!^", strlen("Password successfully changed!^"));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

            unlock_critical_section(&semOp);

            return true;
        }
        else
        {
            // New & reentered passwords don't match
            writeBytes = write(connFD, "The new password and the reentered passwords don't seem to pass!^", strlen("The new password and the reentered passwords don't seem to pass!^"));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        }
    }
    else
    {
        // Password doesn't match with old password
        writeBytes = write(connFD, "The entered password doesn't seem to match with the old password", strlen("The entered password doesn't seem to match with the old password"));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
    }

    unlock_critical_section(&semOp);

    return false;
}

bool lock_critical_section(struct sembuf *semOp)
{
    semOp->sem_flg = SEM_UNDO;
    semOp->sem_op = -1;
    semOp->sem_num = 0;
    int semopStatus = semop(semIdentifier, semOp, 1);
    if (semopStatus == -1)
    {
        perror("Error while locking critical section");
        return false;
    }
    return true;
}

bool unlock_critical_section(struct sembuf *semOp)
{
    semOp->sem_op = 1;
    int semopStatus = semop(semIdentifier, semOp, 1);
    if (semopStatus == -1)
    {
        perror("Error while operating on semaphore!");
        _exit(1);
    }
    return true;
}

void write_transaction_to_array(int *transactionArray, int ID)
{
    // Check if there's any free space in the array to write the new transaction ID
    int iter = 0;
    while (transactionArray[iter] != -1)
        iter++;

    if (iter >= MAX_TRANSACTIONS)
    {
        // No space
        for (iter = 1; iter < MAX_TRANSACTIONS; iter++)
            // Shift elements one step back discarding the oldest transaction
            transactionArray[iter - 1] = transactionArray[iter];
        transactionArray[iter - 1] = ID;
    }
    else
    {
        // Space available
        transactionArray[iter] = ID;
    }
}

int write_transaction_to_file(int accountNumber, long int oldBalance, long int newBalance, bool operation)
{
    struct Transaction newTransaction;
    newTransaction.accountNumber = accountNumber;
    newTransaction.oldBalance = oldBalance;
    newTransaction.newBalance = newBalance;
    newTransaction.operation = operation;
    newTransaction.transactionTime = time(NULL);

    ssize_t readBytes, writeBytes;

    int transactionFileDescriptor = open(TRANSACTION_FILE, O_CREAT | O_APPEND | O_RDWR, S_IRWXU);

    // Get most recent transaction number
    off_t offset = lseek(transactionFileDescriptor, -sizeof(struct Transaction), SEEK_END);
    if (offset >= 0)
    {
        // There exists at least one transaction record
        struct Transaction prevTransaction;
        readBytes = read(transactionFileDescriptor, &prevTransaction, sizeof(struct Transaction));

        newTransaction.transactionID = prevTransaction.transactionID + 1;
    }
    else
        // No transaction records exist
        newTransaction.transactionID = 0;

    writeBytes = write(transactionFileDescriptor, &newTransaction, sizeof(struct Transaction));

    return newTransaction.transactionID;
}

// =====================================================

#endif