#ifndef ADMIN_FUNCTIONS
#define ADMIN_FUNCTIONS

#include "./common.h"
// Function Prototypes =================================

bool admin_operation_handler(int connFD);
bool modify_customer_info(int connFD);
bool add_employee(int connFD);
bool modify_employee_info(int connFD);
bool manage_user_roles(int connFD);

// =====================================================

// Function Definition =================================

// =====================================================

bool admin_operation_handler(int connFD)
{

    if (login_handler(true, connFD, NULL))
    {
        ssize_t writeBytes, readBytes;            // Number of bytes read from / written to the client
        char readBuffer[1000], writeBuffer[1000]; // A buffer used for reading & writing to the client
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "Welcome dear admin!");
        while (1)
        {
            strcat(writeBuffer, "\n");
            strcat(writeBuffer, "1. Add New Bank Employee\n2. Modify Customer Details\n3. Modify Employee Details\n4. Manage User Roles\nPress any other key to logout");
            writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
            if (writeBytes == -1)
            {
                perror("Error while writing ADMIN_MENU to client!");
                return false;
            }
            bzero(writeBuffer, sizeof(writeBuffer));

            readBytes = read(connFD, readBuffer, sizeof(readBuffer));
            if (readBytes == -1)
            {
                perror("Error while reading client's choice for ADMIN_MENU");
                return false;
            }

            int choice = atoi(readBuffer);
            switch (choice)
            {
                case 1:add_employee(connFD);
                    break;
                case 2:modify_customer_info(connFD);
                    break;
                case 3:modify_employee_info(connFD);
                    break;
                case 4:manage_user_roles(connFD);
                    break;

               default:
                writeBytes = write(connFD, "Logging you out now !$", strlen("Logging you out now !$"));
                return false;
            }
        }
    }
    else
    {
        // ADMIN LOGIN FAILED
        return false;
    }
    return true;
}

bool add_employee(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];

    struct Employee newEmployee, previousEmployee;

    int employeeFileDescriptor = open(EMPLOYEE_FILE, O_RDONLY);
    if (employeeFileDescriptor == -1 && errno == ENOENT)
    {
        // Employee file was never created
        newEmployee.id = 0;
    }
    else if (employeeFileDescriptor == -1)
    {
        perror("Error while opening employee file");
        return -1;
    }
    else
    {
        int offset = lseek(employeeFileDescriptor, -sizeof(struct Employee), SEEK_END);
        if (offset == -1)
        {
            perror("Error seeking to last Employee record!");
            return false;
        }

        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Employee), getpid()};
        int lockingStatus = fcntl(employeeFileDescriptor, F_SETLKW, &lock);
        if (lockingStatus == -1)
        {
            perror("Error obtaining read lock on Employee record!");
            return false;
        }

        readBytes = read(employeeFileDescriptor, &previousEmployee, sizeof(struct Employee));
        if (readBytes == -1)
        {
            perror("Error while reading Employee record from file!");
            return false;
        }

        lock.l_type = F_UNLCK;
        fcntl(employeeFileDescriptor, F_SETLK, &lock);

        close(employeeFileDescriptor);

        newEmployee.id = previousEmployee.id + 1;
    }
    newEmployee.isMng = 0;
        sprintf(writeBuffer, "%s%s", "Enter the details for the employee\n", "What is the employee's name?");
   
    writeBytes = write(connFD, writeBuffer, sizeof(writeBuffer));
    if (writeBytes == -1)
    {
        perror("Error writing ADMIN_ADD_EMPLOYEE_NAME message to client!");
        return false;
    }

    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1)
    {
        perror("Error reading employee name response from client!");
        ;
        return false;
    }

    strcpy(newEmployee.name, readBuffer);

    writeBytes = write(connFD, "What is the employee's gender?\nEnter M for male, F for female and O for others", strlen("What is the employee's gender?\nEnter M for male, F for female and O for others"));
    if (writeBytes == -1)
    {
        perror("Error writing ADMIN_ADD_EMPLOYEE_GENDER message to client!");
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1)
    {
        perror("Error reading employee gender response from client!");
        return false;
    }

    if (readBuffer[0] == 'M' || readBuffer[0] == 'F' || readBuffer[0] == 'O')
        newEmployee.gender = readBuffer[0];
    else
    {
        writeBytes = write(connFD, "It seems you've enter a wrong gender choice!\nYou'll now be redirected to the main menu!^", strlen("It seems you've enter a wrong gender choice!\nYou'll now be redirected to the main menu!^"));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }

    bzero(writeBuffer, sizeof(writeBuffer));
    strcpy(writeBuffer, "What is the employee's age?");
    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
    if (writeBytes == -1)
    {
        perror("Error writing ADMIN_ADD_EMPLOYEE_AGE message to client!");
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1)
    {
        perror("Error readingemployee age response from client!");
        return false;
    }

    int employeeAge = atoi(readBuffer);
    if (employeeAge == 0)
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
    newEmployee.age = employeeAge;

    strcpy(newEmployee.login, newEmployee.name);
    strcat(newEmployee.login, "-");
    sprintf(writeBuffer, "%d", newEmployee.id);
    strcat(newEmployee.login, writeBuffer);

    memset(newEmployee.loanID, -1, MAX_LOANS * sizeof(int));

    char hashedPassword[1000];
    strcpy(hashedPassword, crypt("emp@123", SALT_BAE));
    strcpy(newEmployee.password, hashedPassword);

    employeeFileDescriptor = open(EMPLOYEE_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);
    if (employeeFileDescriptor == -1)
    {
        perror("Error while creating / opening employee file!");
        return false;
    }
    writeBytes = write(employeeFileDescriptor, &newEmployee, sizeof(newEmployee));
    if (writeBytes == -1)
    {
        perror("Error while writing Employee record to file!");
        return false;
    }

    close(employeeFileDescriptor);

    bzero(writeBuffer, sizeof(writeBuffer));
    sprintf(writeBuffer, "%s%s-%d\n%s%s", "The autogenerated login ID for the employee is : ", newEmployee.name, newEmployee.id, "The autogenerated password for the employee is : ", "emp@123");
    strcat(writeBuffer, "^");
    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
    if (writeBytes == -1)
    {
        perror("Error sending employee loginID and password to the client!");
        return false;
    }

    readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

    return newEmployee.id;
}

bool modify_customer_info(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];

    struct Customer customer;

    int customerID;

    off_t offset;
    int lockingStatus;

    writeBytes = write(connFD, "Enter the ID of the customer who's information you want to edit", strlen("Enter the ID of the customer who's information you want to edit"));

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));

    customerID = atoi(readBuffer);

    int customerFileDescriptor = open(CUSTOMER_FILE, O_RDONLY);
    if (customerFileDescriptor == -1)
    {
        // Customer File doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "No customer could be found for the given ID");
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));

        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    
    offset = lseek(customerFileDescriptor, customerID * sizeof(struct Customer), SEEK_SET);
    if (errno == EINVAL)
    {
        // Customer record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "No customer could be found for the given ID");
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));

        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    else if (offset == -1)
    {
        perror("Error while seeking to required customer record!");
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};

    // Lock the record to be read
    lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1)
    {
        perror("Couldn't obtain lock on customer record!");
        return false;
    }

    readBytes = read(customerFileDescriptor, &customer, sizeof(struct Customer));
    if (readBytes == -1)
    {
        perror("Error while reading customer record from the file!");
        return false;
    }

    // Unlock the record
    lock.l_type = F_UNLCK;
    fcntl(customerFileDescriptor, F_SETLK, &lock);

    close(customerFileDescriptor);

    writeBytes = write(connFD, "Which information would you like to modify?\n1. Name 2. Age 3. Gender \nPress any other key to cancel", strlen("Which information would you like to modify?\n1. Name 2. Age 3. Gender \nPress any other key to cancel"));

    readBytes = read(connFD, readBuffer, sizeof(readBuffer));


    int choice = atoi(readBuffer);
    if (choice == 0)
    { // A non-numeric string was passed to atoi
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

    bzero(readBuffer, sizeof(readBuffer));
    switch (choice)
    {
    case 1:
        writeBytes = write(connFD, "What's the updated value for name?", strlen("What's the updated value for name?"));

        readBytes = read(connFD, &readBuffer, sizeof(readBuffer));

        strcpy(customer.name, readBuffer);
        break;
    case 2:
        writeBytes = write(connFD, "What's the updated value for age?", strlen("What's the updated value for age?"));
  
        readBytes = read(connFD, &readBuffer, sizeof(readBuffer));

        int updatedAge = atoi(readBuffer);
        if (updatedAge == 0)
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
        customer.age = updatedAge;
        break;
    case 3:
        writeBytes = write(connFD, "What's the updated value for gender?", strlen("What's the updated value for gender?"));

        readBytes = read(connFD, &readBuffer, sizeof(readBuffer));

        customer.gender = readBuffer[0];
        break;
    default:
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "It seems you've made an invalid menu choice\nYou'll now be redirected to the main menu!^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));

        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }

    customerFileDescriptor = open(CUSTOMER_FILE, O_WRONLY);
    if (customerFileDescriptor == -1)
    {
        perror("Error while opening customer file");
        return false;
    }
    offset = lseek(customerFileDescriptor, customerID * sizeof(struct Customer), SEEK_SET);
    if (offset == -1)
    {
        perror("Error while seeking to required customer record!");
        return false;
    }

    lock.l_type = F_WRLCK;
    lock.l_start = offset;
    lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1)
    {
        perror("Error while obtaining write lock on customer record!");
        return false;
    }

    writeBytes = write(customerFileDescriptor, &customer, sizeof(struct Customer));


    lock.l_type = F_UNLCK;
    fcntl(customerFileDescriptor, F_SETLKW, &lock);

    close(customerFileDescriptor);

    writeBytes = write(connFD, "The required modification was successfully made!\nYou'll now be redirected to the main menu!^", strlen("The required modification was successfully made!\nYou'll now be redirected to the main menu!^"));

    readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

    return true;
}

bool modify_employee_info(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];

    struct Employee employee;

    int employeeID;

    off_t offset;
    int lockingStatus;

    writeBytes = write(connFD, "Enter the ID of the employee who's information you want to edit", strlen("Enter the ID of the employee who's information you want to edit"));

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));

    employeeID = atoi(readBuffer);

    int employeeFileDescriptor = open(EMPLOYEE_FILE, O_RDONLY);
    if (employeeFileDescriptor == -1)
    {
        // Employee File doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "No employee could be found for the given ID");
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing EMPlOYEE_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    
    offset = lseek(employeeFileDescriptor, employeeID * sizeof(struct Employee), SEEK_SET);
    if (errno == EINVAL)
    {
        // Employee record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "No employee could be found for the given ID");
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing EMPLOYEE_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    else if (offset == -1)
    {
        perror("Error while seeking to required Employee record!");
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Employee), getpid()};

    // Lock the record to be read
    lockingStatus = fcntl(employeeFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1)
    {
        perror("Couldn't obtain lock on employee record!");
        return false;
    }

    readBytes = read(employeeFileDescriptor, &employee, sizeof(struct Employee));
    if (readBytes == -1)
    {
        perror("Error while reading employee record from the file!");
        return false;
    }

    // Unlock the record
    lock.l_type = F_UNLCK;
    fcntl(employeeFileDescriptor, F_SETLK, &lock);

    close(employeeFileDescriptor);

    writeBytes = write(connFD, "Which information would you like to modify?\n1. Name 2. Age 3. Gender \nPress any other key to cancel", strlen("Which information would you like to modify?\n1. Name 2. Age 3. Gender \nPress any other key to cancel"));

    readBytes = read(connFD, readBuffer, sizeof(readBuffer));


    int choice = atoi(readBuffer);
    if (choice == 0)
    { // A non-numeric string was passed to atoi
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

    bzero(readBuffer, sizeof(readBuffer));
    switch (choice)
    {
    case 1:
        writeBytes = write(connFD, "What's the updated value for name?", strlen("What's the updated value for name?"));

        readBytes = read(connFD, &readBuffer, sizeof(readBuffer));

        strcpy(employee.name, readBuffer);
        break;
    case 2:
        writeBytes = write(connFD, "What's the updated value for age?", strlen("What's the updated value for age?"));

        readBytes = read(connFD, &readBuffer, sizeof(readBuffer));

        int updatedAge = atoi(readBuffer);
        if (updatedAge == 0)
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
        employee.age = updatedAge;
        break;
    case 3:
        writeBytes = write(connFD, "What's the updated value for gender?", strlen("What's the updated value for gender?"));

        readBytes = read(connFD, &readBuffer, sizeof(readBuffer));

        employee.gender = readBuffer[0];
        break;
    default:
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "It seems you've made an invalid menu choice\nYou'll now be redirected to the main menu!^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));

        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }

    employeeFileDescriptor = open(EMPLOYEE_FILE, O_WRONLY);
    if (employeeFileDescriptor == -1)
    {
        perror("Error while opening employee file");
        return false;
    }
    offset = lseek(employeeFileDescriptor, employeeID * sizeof(struct Employee), SEEK_SET);
    if (offset == -1)
    {
        perror("Error while seeking to required employee record!");
        return false;
    }

    lock.l_type = F_WRLCK;
    lock.l_start = offset;
    lockingStatus = fcntl(employeeFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1)
    {
        perror("Error while obtaining write lock on employee record!");
        return false;
    }

    writeBytes = write(employeeFileDescriptor, &employee, sizeof(struct Employee));


    lock.l_type = F_UNLCK;
    fcntl(employeeFileDescriptor, F_SETLKW, &lock);

    close(employeeFileDescriptor);

    writeBytes = write(connFD, "The required modification was successfully made!\nYou'll now be redirected to the main menu!^", strlen("The required modification was successfully made!\nYou'll now be redirected to the main menu!^"));

    readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

    return true;
}

bool manage_user_roles(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];

    struct Employee employee;

    int employeeID;

    off_t offset;
    int lockingStatus;

    writeBytes = write(connFD, "Enter the ID of the employee who would get promoted to manager post", strlen("Enter the ID of the employee who would get promoted to manager post"));

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));


    employeeID = atoi(readBuffer);

    int employeeFileDescriptor = open(EMPLOYEE_FILE, O_RDONLY);
    if (employeeFileDescriptor == -1)
    {
        // Employee File doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "No employee could be found for the given ID");
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));

        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    
    offset = lseek(employeeFileDescriptor, employeeID * sizeof(struct Employee), SEEK_SET);
    if (errno == EINVAL)
    {
        // Employee record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "No employee could be found for the given ID");
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
 
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    else if (offset == -1)
    {
        perror("Error while seeking to required Employee record!");
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Employee), getpid()};

    // Lock the record to be read
    lockingStatus = fcntl(employeeFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1)
    {
        perror("Couldn't obtain lock on employee record!");
        return false;
    }

    readBytes = read(employeeFileDescriptor, &employee, sizeof(struct Employee));


    // Unlock the record
    lock.l_type = F_UNLCK;
    fcntl(employeeFileDescriptor, F_SETLK, &lock);

    close(employeeFileDescriptor);

    writeBytes = write(connFD, "What would you like to make him ?\n1. Manager 2. Remain as employee \nPress any other key to cancel", strlen("What would you like to make him ?\n1. Manager 2. Remain as employee \nPress any other key to cancel"));

    readBytes = read(connFD, readBuffer, sizeof(readBuffer));


    int choice = atoi(readBuffer);
    if (choice == 0)
    { // A non-numeric string was passed to atoi
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

    bzero(readBuffer, sizeof(readBuffer));
    switch (choice)
    {
    case 1:
        employee.isMng=1;
        break;
    case 2:
        employee.isMng=0;
        break;
   
    default:
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "It seems you've made an invalid menu choice\nYou'll now be redirected to the main menu!^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));

        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }

    employeeFileDescriptor = open(EMPLOYEE_FILE, O_WRONLY);
    if (employeeFileDescriptor == -1)
    {
        perror("Error while opening employee file");
        return false;
    }
    offset = lseek(employeeFileDescriptor, employeeID * sizeof(struct Employee), SEEK_SET);
    if (offset == -1)
    {
        perror("Error while seeking to required employee record!");
        return false;
    }

    lock.l_type = F_WRLCK;
    lock.l_start = offset;
    lockingStatus = fcntl(employeeFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1)
    {
        perror("Error while obtaining write lock on employee record!");
        return false;
    }

    writeBytes = write(employeeFileDescriptor, &employee, sizeof(struct Employee));


    lock.l_type = F_UNLCK;
    fcntl(employeeFileDescriptor, F_SETLKW, &lock);

    close(employeeFileDescriptor);

    writeBytes = write(connFD, "The required modification was successfully made!\nYou'll now be redirected to the main menu!^", strlen("The required modification was successfully made!\nYou'll now be redirected to the main menu!^"));

    readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

    return true;
}


#endif