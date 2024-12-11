#ifndef EMPLOYEE_FUNCTIONS
#define EMPLOYEE_FUNCTIONS

#include <sys/ipc.h>
#include <sys/sem.h>

struct Employee loggedInEmployee;
int semIdentifier;

bool add_account(int connFD);
int add_customer(int connFD, int newAccountNumber);
bool employee_operation_handler(int connFD);
bool change_employee_password(int connFD);
bool get_assigned_loans(int connFD, int employeeNumber);
bool approve_reject_loans(int connFD, int employeeNumber);
bool loan_deposit(int connFD, int accountNumber, long int amount);

bool employee_operation_handler(int connFD)
{
    if (login_handler_2(3, connFD, &loggedInEmployee))
    {   printf("Employee Logged in\n");
        ssize_t writeBytes, readBytes;            // Number of bytes read from / written to the client
        char readBuffer[1000], writeBuffer[1000]; // A buffer used for reading & writing to the client

        // Get a semaphore for the user
        key_t semKey = ftok(EMPLOYEE_FILE, loggedInEmployee.id); // Generate a key based on the id hence, different people will have different semaphores

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
        strcpy(writeBuffer, "Welcome Employee!");
        while (1)
        {
            strcat(writeBuffer, "\n");
            strcat(writeBuffer, "1. Add New Customer \n2. Modify Customer Details \n3. Approve/Reject Loans \n4. View Assigned Loans \n5. View Customer Transactions \n6. Change Password \nPress any other key to logout");
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
                perror("Error while reading client's choice for EMPLOYEE_MENU");
                return false;
            }
            
            int choice = atoi(readBuffer);
            switch (choice)
            {
            case 1:
                add_account(connFD);
                break;
            case 2:
                modify_customer_info(connFD);
                break;
            case 3:
                approve_reject_loans(connFD, loggedInEmployee.id);
                break;
            case 4:
                get_assigned_loans(connFD, loggedInEmployee.id);
                break;
            case 5:
                get_transaction_details(connFD, -1);
                break; 
            case 6:
                change_employee_password(connFD);
                break;
            default:
                writeBytes = write(connFD, "Logging you out now dear employee !$", strlen("Logging you out now dear employee !$"));
                return false;
            }
        }
    }
    else
    {
        // EMPLOYEE LOGIN FAILED
        return false;
    }
    return true;
}

bool get_assigned_loans(int connFD, int employeeNumber)
{

    ssize_t readBytes, writeBytes;                               // Number of bytes read from / written to the socket
    char readBuffer[1000], writeBuffer[10000], tempBuffer[1000]; // A buffer for reading from / writing to the socket
    struct Employee employee;

    employee.id = employeeNumber;

    if (get_employee_details(connFD, &employee))
    {
        int iter;
        struct Loan loan;

        bzero(writeBuffer, sizeof(writeBuffer));

        int loanFileDescriptor = open(LOAN_FILE, O_RDONLY);
        if (loanFileDescriptor == -1)
        {
            perror("Error while opening loan file!");
            write(connFD, "No loan were present on this bank!^", strlen("No loan were present on this bank!^"));
            read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
            return false;
        }

        for (iter = 0; iter < MAX_LOANS && employee.loanID[iter] != -1; iter++)
        {
            int offset = lseek(loanFileDescriptor, employee.loanID[iter] * sizeof(struct Loan), SEEK_SET);

            struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Loan), getpid()};
            fcntl(loanFileDescriptor, F_SETLKW, &lock);

            readBytes = read(loanFileDescriptor, &loan, sizeof(struct Loan));

            lock.l_type = F_UNLCK;
            fcntl(loanFileDescriptor, F_SETLK, &lock);

            bzero(tempBuffer, sizeof(tempBuffer));
            sprintf(tempBuffer, "Loan ID : %d \n Customer ID : %d \n Amount : %ld \n Approved Status : %s \n Assigned Employee ID : %d \n\n", loan.loanID, loan.customerID , loan.amount, (loan.approved==0? "Pending" : (loan.approved==1? "Approved": "Rejected") ) , loan.EmployeeID);

            if (strlen(writeBuffer) == 0)
                strcpy(writeBuffer, tempBuffer); //first transaction so copy it
            else
                strcat(writeBuffer, tempBuffer);  // adding more transactions in it
        }

        close(loanFileDescriptor);

        if (strlen(writeBuffer) == 0)
        {
            write(connFD, "No loans are presently assigned on this employee!^", strlen("No loans are presently assigned on this employee!^"));
            read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
            return false;
        }
        else
        {
            strcat(writeBuffer, "^");
            writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
            read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        }
    }
}

//reading loan from loan file using loanId in emp structure and then approving/rejecting, updating loan array in emp structure 
bool approve_reject_loans(int connFD, int employeeNumber)
{
    ssize_t readBytes, writeBytes;                               // Number of bytes read from / written to the socket
    char readBuffer[1000], writeBuffer[1000], tempBuffer[1000]; // A buffer for reading from / writing to the socket
    struct Employee employee;

    bzero(writeBuffer, sizeof(writeBuffer));
    bzero(readBuffer, sizeof(readBuffer));
    bzero(tempBuffer, sizeof(tempBuffer));

    employee.id = employeeNumber;

    if(get_employee_details(connFD, &employee))
    {
        if(employee.loanID[0]!=-1)
        {
            struct Loan loan;

            //read one loan from assigned loans
            int loanFileDescriptor = open(LOAN_FILE, O_RDONLY);
            off_t offset = lseek(loanFileDescriptor, employee.loanID[0] * sizeof(struct Loan), SEEK_SET);

            //locking
            struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Loan), getpid()};
            fcntl(loanFileDescriptor, F_SETLKW, &lock);

            readBytes = read(loanFileDescriptor, &loan, sizeof(struct Loan));

            //unlocking
            lock.l_type = F_UNLCK;
            fcntl(loanFileDescriptor, F_SETLK, &lock);
            close(loanFileDescriptor);

            //storing loan in tempbuffer to display
            sprintf(tempBuffer, "Loan ID : %d \n Customer ID : %d \n Amount : %ld \n Approved Status : %s \n Assigned Employee ID : %d \n\n", loan.loanID, loan.customerID , loan.amount, (loan.approved==0? "Pending" : (loan.approved==1? "Approved": "Rejected") ) , loan.EmployeeID);

            strcpy(writeBuffer, tempBuffer);
            strcat(writeBuffer, "\n\n");
            strcat(writeBuffer, "Enter 1. to Approve the loan or 2. To Reject the loan");
            
            writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer));

            int choice = atoi(readBuffer);

            bzero(writeBuffer, sizeof(writeBuffer));
            bzero(readBuffer, sizeof(readBuffer));
            switch (choice)
            {
            case 1:
                loan.approved=1;
                // make a function to deposit money in customer's account
                loan_deposit(connFD, loan.customerID, loan.amount);
                break;
            case 2:
                loan.approved=-1;
                break;
            default:
                strcpy(writeBuffer, "You have entered an invalid number! \nYou'll now be redirected to the main menu!^");
                writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
                readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
                return false;
            }

            //update in loan file
            loanFileDescriptor = open(LOAN_FILE, O_WRONLY);
            offset = lseek(loanFileDescriptor, employee.loanID[0] * sizeof(struct Loan), SEEK_SET);

            lock.l_type = F_WRLCK;
            lock.l_start = offset;
            fcntl(loanFileDescriptor, F_SETLKW, &lock);
            writeBytes = write(loanFileDescriptor, &loan, sizeof(struct Loan));
            lock.l_type = F_UNLCK;
            fcntl(loanFileDescriptor, F_SETLKW, &lock);
            close(loanFileDescriptor);

            //update the loan array of employee
// This piece of code removes the first loan ID from the list and shifts all remaining loan IDs left by one position. The last position is then marked as unassigned.           
            int curr_present_loans=0;
            while(curr_present_loans<MAX_LOANS && employee.loanID[curr_present_loans]!=-1)
            curr_present_loans++;

            for(int i=1;i<curr_present_loans;i++)
                employee.loanID[i-1]=employee.loanID[i];
            
            employee.loanID[curr_present_loans-1]=-1;

            //update in employee file
            int employeeFileDescriptor = open(EMPLOYEE_FILE, O_WRONLY);
            off_t offset2 = lseek(employeeFileDescriptor, employee.id * sizeof(struct Employee), SEEK_SET);
            struct flock lock2 = {F_WRLCK, SEEK_SET, offset2, sizeof(struct Employee), getpid()};
            fcntl(employeeFileDescriptor, F_SETLKW, &lock2);
            writeBytes = write(employeeFileDescriptor, &employee, sizeof(struct Employee));
            lock2.l_type = F_UNLCK;
            fcntl(employeeFileDescriptor, F_SETLKW, &lock2);
            close(employeeFileDescriptor);


            writeBytes = write(connFD, "The Loan processing was successfully done! \nYou'll now be redirected to the main menu!^", strlen("The Loan processing was successfully done! \nYou'll now be redirected to the main menu!^"));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
            return true;
        }
        else
        {
            //no loans assigned
            write(connFD, "No loans are presently assigned!^", strlen("No loans are presently assigned!^"));
            read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
            return false;
        }
    }
}

bool loan_deposit(int connFD, int accNumber, long int amount)
{
    char readBuffer[1000], writeBuffer[1000];
    ssize_t readBytes, writeBytes;
    struct Account account;
    account.accountNumber = accNumber;

    long int depositAmount = amount;

    if (get_account_details(connFD, &account))
    {
        if (account.active)
        {
                int newTransactionID = write_transaction_to_file(account.accountNumber, account.balance, account.balance + depositAmount, 1);
                write_transaction_to_array(account.transactions, newTransactionID);

                account.balance += depositAmount;

                //updating in the balance in account file
                int accountFileDescriptor = open(ACCOUNT_FILE, O_WRONLY);
                off_t offset3 = lseek(accountFileDescriptor, account.accountNumber * sizeof(struct Account), SEEK_SET);

                struct flock lock3 = {F_WRLCK, SEEK_SET, offset3, sizeof(struct Account), getpid()};
                fcntl(accountFileDescriptor, F_SETLKW, &lock3);

                writeBytes = write(accountFileDescriptor, &account, sizeof(struct Account));
                if (writeBytes == -1)
                {
                    perror("Error storing updated deposit money in account record!");
                    return false;
                }

                lock3.l_type = F_UNLCK;
                fcntl(accountFileDescriptor, F_SETLK, &lock3);

                write(connFD, "The loan amount has been successfully added!^", strlen("The loan amount has been successfully added!^"));
                read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
                return true;
        }
        else
            write(connFD, "It seems the account has been deactivated!^", strlen("It seems the account has been deactivated!^"));

        read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    else
    {
        // FAIL
        return false;
    }
}

bool add_account(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];

    struct Account newAccount, prevAccount;

    int accountFileDescriptor = open(ACCOUNT_FILE, O_RDONLY);
    if (accountFileDescriptor == -1 && errno == ENOENT)
    {
        // Account file was never created
        newAccount.accountNumber = 0;
    }
    else if (accountFileDescriptor == -1)
    {
        perror("Error while opening account file");
        return false;
    }
    else
    {
        int offset = lseek(accountFileDescriptor, -sizeof(struct Account), SEEK_END);
        if (offset == -1)
        {
            perror("Error seeking to last Account record!");
            return false;
        }

        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
        int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
        if (lockingStatus == -1)
        {
            perror("Error obtaining read lock on Account record!");
            return false;
        }

        readBytes = read(accountFileDescriptor, &prevAccount, sizeof(struct Account));
        if (readBytes == -1)
        {
            perror("Error while reading Account record from file!");
            return false;
        }

        lock.l_type = F_UNLCK;
        fcntl(accountFileDescriptor, F_SETLK, &lock);

        close(accountFileDescriptor);

        newAccount.accountNumber = prevAccount.accountNumber + 1;
    }
    
    newAccount.isRegularAccount = 1;

    newAccount.owner = add_customer(connFD, newAccount.accountNumber);

    newAccount.active = true;
    newAccount.balance = 0;

    memset(newAccount.transactions, -1, MAX_TRANSACTIONS * sizeof(int));

    accountFileDescriptor = open(ACCOUNT_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);
    if (accountFileDescriptor == -1)
    {
        perror("Error while creating / opening account file!");
        return false;
    }

    writeBytes = write(accountFileDescriptor, &newAccount, sizeof(struct Account));
    if (writeBytes == -1)
    {
        perror("Error while writing Account record to file!");
        return false;
    }

    close(accountFileDescriptor);

    bzero(writeBuffer, sizeof(writeBuffer));
    sprintf(writeBuffer, "%s%d", "The newly created account's number is :", newAccount.accountNumber);
    strcat(writeBuffer, "\nRedirecting you to the main menu ...^");
    writeBytes = write(connFD, writeBuffer, sizeof(writeBuffer));
    readBytes = read(connFD, readBuffer, sizeof(read)); // Dummy read
    return true;
}

int add_customer(int connFD, int newAccountNumber)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];

    struct Customer newCustomer, previousCustomer;

    int customerFileDescriptor = open(CUSTOMER_FILE, O_RDONLY);
    if (customerFileDescriptor == -1 && errno == ENOENT)
    {
        // Customer file was never created
        newCustomer.id = 0;
    }
    else if (customerFileDescriptor == -1)
    {
        perror("Error while opening customer file");
        return -1;
    }
    else
    {
        int offset = lseek(customerFileDescriptor, -sizeof(struct Customer), SEEK_END);
        if (offset == -1)
        {
            perror("Error seeking to last Customer record!");
            return false;
        }

        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};
        int lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
        if (lockingStatus == -1)
        {
            perror("Error obtaining read lock on Customer record!");
            return false;
        }

        readBytes = read(customerFileDescriptor, &previousCustomer, sizeof(struct Customer));
        if (readBytes == -1)
        {
            perror("Error while reading Customer record from file!");
            return false;
        }

        lock.l_type = F_UNLCK;
        fcntl(customerFileDescriptor, F_SETLK, &lock);

        close(customerFileDescriptor);

        newCustomer.id = previousCustomer.id + 1;
    }

        sprintf(writeBuffer, "%s%s", "Enter the details for the customer\n", "What is the customer's name?");

    writeBytes = write(connFD, writeBuffer, sizeof(writeBuffer));

    readBytes = read(connFD, readBuffer, sizeof(readBuffer));

    strcpy(newCustomer.name, readBuffer);

    writeBytes = write(connFD, "What is the customer's gender?\nEnter M for male, F for female and O for others", strlen("What is the customer's gender?\nEnter M for male, F for female and O for others"));
 
    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));

    if (readBuffer[0] == 'M' || readBuffer[0] == 'F' || readBuffer[0] == 'O')
        newCustomer.gender = readBuffer[0];
    else
    {
        writeBytes = write(connFD, "It seems you've enter a wrong gender choice!\nYou'll now be redirected to the main menu!^", strlen("It seems you've enter a wrong gender choice!\nYou'll now be redirected to the main menu!^"));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }

    bzero(writeBuffer, sizeof(writeBuffer));
    strcpy(writeBuffer, "What is the customer's age?");
    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));


    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1)
    {
        perror("Error reading customer age response from client!");
        return false;
    }

    int customerAge = atoi(readBuffer);
    if (customerAge == 0)
    {
        // Either client has sent age as 0 (which is invalid) or has entered a non-numeric string
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "It seems you have passed a sequence of alphabets when a number was expected or you have entered an invalid number!\nYou'll now be redirected to the main menu!^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing ERRON_INPUT_FOR_NUMBER message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    newCustomer.age = customerAge;

    newCustomer.account = newAccountNumber;

    strcpy(newCustomer.login, newCustomer.name);
    strcat(newCustomer.login, "-");
    sprintf(writeBuffer, "%d", newCustomer.id);
    strcat(newCustomer.login, writeBuffer);

    char hashedPassword[1000];
    strcpy(hashedPassword, crypt("abc@123", SALT_BAE));
    strcpy(newCustomer.password, hashedPassword);

    customerFileDescriptor = open(CUSTOMER_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);
    if (customerFileDescriptor == -1)
    {
        perror("Error while creating / opening customer file!");
        return false;
    }
    writeBytes = write(customerFileDescriptor, &newCustomer, sizeof(newCustomer));
    if (writeBytes == -1)
    {
        perror("Error while writing Customer record to file!");
        return false;
    }

    close(customerFileDescriptor);

    bzero(writeBuffer, sizeof(writeBuffer));
    sprintf(writeBuffer, "%s%s-%d\n%s%s", "The autogenerated login ID for the customer is : ", newCustomer.name, newCustomer.id, "The autogenerated password for the customer is : ", "abc@123");
    strcat(writeBuffer, "^");
    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
    if (writeBytes == -1)
    {
        perror("Error sending customer loginID and password to the client!");
        return false;
    }

    readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

    return newCustomer.id;
}

bool change_employee_password(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000], hashedPassword[1000];

    char newPassword[1000];

    writeBytes = write(connFD, "Enter your old password", strlen("Enter your old password"));
    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));

    if (strcmp(crypt(readBuffer, SALT_BAE), loggedInEmployee.password) == 0)
    {
        // Password matches with old password
        writeBytes = write(connFD, "Enter the new password", strlen("Enter the new password"));
        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        
        //hasing new password
        strcpy(newPassword, crypt(readBuffer, SALT_BAE));

        writeBytes = write(connFD, "Reenter the new password", strlen("Reenter the new password"));
        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));

        if (strcmp(crypt(readBuffer, SALT_BAE), newPassword) == 0)
        {
           strcpy(loggedInEmployee.password, newPassword);

            int employeeFileDescriptor = open(EMPLOYEE_FILE, O_WRONLY);

            off_t offset = lseek(employeeFileDescriptor, loggedInEmployee.id * sizeof(struct Employee), SEEK_SET);
            if (offset == -1)
            {
                perror("Error seeking to the employee record!");
                return false;
            }

            struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Employee), getpid()};
            int lockingStatus = fcntl(employeeFileDescriptor, F_SETLKW, &lock);

            writeBytes = write(employeeFileDescriptor, &loggedInEmployee, sizeof(struct Employee));
            if (writeBytes == -1)
            {
                perror("Error storing updated employee password into employee record!");
                return false;
            }

            lock.l_type = F_UNLCK;
            lockingStatus = fcntl(employeeFileDescriptor, F_SETLK, &lock);
            close(employeeFileDescriptor);

            writeBytes = write(connFD, "Password successfully changed!^", strlen("Password successfully changed!^"));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
            return true;
        }
        else
        {
            // New & reentered passwords don't match
            writeBytes = write(connFD, "New password and Reentered password don't match!^", strlen("New password and Reentered password don't match!^"));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        }
    }
    else
    {
        // Password doesn't match with old password
        writeBytes = write(connFD, "The entered password doesn't match with the old password!^", strlen("The entered password doesn't match with the old password!^"));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
    }
    return false;
}





#endif