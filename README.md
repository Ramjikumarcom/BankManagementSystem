## International Institute of Information Technology, Bangalore
## CS-513 System Software – Mini Project
## Title: Design and Development of Banking Management System
 
The Banking Management System aims to simulate the core functionalities of a bank, including 
customer account management, transactions, loan processing, and administrative oversight. 
The system features secure login mechanisms and role-based access for customers, 
employees, managers, and administrators. The focus is on ensuring data consistency, security, 
and correct handling of race conditions using file management, locking mechanisms, and 
process synchronization. 

Roles: 

    1. Customer: Customers log in using their credentials and have access to banking 
    operations such as viewing account balances, depositing or withdrawing money, 
    transferring funds, and applying for loans. The system ensures proper locking 
    mechanisms to avoid race conditions when performing concurrent operations. 
        o Login System (one session per user) 
        o View Account Balance 
        o Deposit Money 
        o Withdraw Money 
        o Transfer Funds 
        o Apply for a Loan 
        o Change Password 
        o Adding Feedback 
        o View Transaction History 
        o Logout 
        o Exit 

    2. Bank Employee: Bank employees manage customer accounts, including opening, 
    modifying, or closing accounts, as well as processing loans. 
        o Login System (one session per user) 
        o Add New Customer 
        o Modify Customer Details 
        o Process Loan Applications 
        o Approve/Reject Loans 
        o View Assigned Loan Applications 
        o View Customer Transactions( Passbook Kind of feature) 
        o Change Password 
        o Logout 
        o Exit 
        
    3. Manager: 
        o Login System (one session per user) 
        o Activate/Deactivate Customer Accounts 
        o Assign Loan Application Processes to Employees 
        o Review Customer Feedback 
        o Change Password 
        o Logout 
        o Exit 

    4. Administrator: The Administrator has the highest level of access and manages both 
    customer and employee accounts. 
        o Login System (one session per user) 
        o Add New Bank Employee 
        o Modify Customer/Employee Details 
        o Manage User Roles
        o Logout
        o Exit 


Technical Requirements: 

o Socket Programming: The server maintains the database and serves multiple clients 
                      concurrently. Clients can connect to the server and access their specific account details. 
o System Calls: Use system calls instead of library functions wherever possible, including 
                process management, file management, file locking, semaphores, multithreading, and 
                inter-process communication mechanisms.

 