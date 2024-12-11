#ifndef EMPLOYEE_RECORD
#define EMPLOYEE_RECORD

#define MAX_LOANS 10
#include <stdbool.h>

struct Employee
{
    int id; // 0, 1, 2 ....
    char name[25];
    char gender; // M -> Male, F -> Female, O -> Other
    int age;
    // Login Credentials
    char login[30]; // Format : name-id (name will the first word in the structure member `name`)
    char password[30];
    // Bank data
    int loanID[MAX_LOANS]; // Number of loans assigned with loanID's , loanID[MAX_LOANS]==-1 means loan is not assigned yet
    bool isMng;  //0 ->employee, 1->manager
};

#endif