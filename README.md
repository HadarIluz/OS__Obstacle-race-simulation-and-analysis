# Operating-Systems

Obstacle race - simulation and analysis

The program simulates launching trainees on an obstacle course and measuring trainee times on each obstacle. The trainees perform all types of obstacles in the maximum possible parallelism, each trainee is a thread. If there is no free obstacle the thread enters the waiting list until another trainee completes a course and then he will try his luck to catch an obstacle he has not yet performed to complete the course !.

Linux- C - Multithreading

Requirements: 
• Building the route on the obstacles. 
• Building a group of trainees. 
• Building a waiting list  
      - building a convenient mechanism for adding and removing trainees 
      - for actions that may be performed many times. Secured by a lock. 
• Running a workout. 
• Gathering information for each trainee that includes: 
        o The time the training started for him. 
        o The time at which he approached each of the obstacles. 
        o The time at which each of them finished. 
        o The time at which he completed the last hurdle. 
        o Its overall execution time. 
        o The waiting time he spent in "Fortress" because there was not a single vacant facility he had to go through.
