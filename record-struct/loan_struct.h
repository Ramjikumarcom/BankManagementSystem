#ifndef LOAN_RECORD
#define LOAN_RECORD


struct Loan {
    int loanID;              // Unique loan ID
    int customerID;          // ID of the customer requesting the loan
    long int amount;         // Loan amount requested
    int approved;            // Approval status: 0 -> Waiting, 1 -> Approved, -1 -> Rejected
    int  EmployeeID;         // Employee assigned
};

#endif