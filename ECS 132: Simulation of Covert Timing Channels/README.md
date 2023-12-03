## Simulation of Covert Timing Channels

Course: ECS 132 - Probability and Statistics in Computer Science

#### Project Specifications:
  - As described by the instructions in the jupyter notebook, I was tasked with simulating the transmission of a secret message through the altering of inter-packet
delay times of buffered packets from some theoretical network traffic.
  - When simulating the buffering and intentional delays of packets, I varied multiple properties:
    - Whether the delay time of incoming packets followed a unform or exponential distibution
    - The size of the secret message between 16 bits and 32 bits
    - The number of packets in the buffer before starting to send packets with altered inter-packet delay times (varied in the range i=[2,6,10,14,18])
  - After determining the rate of buffer underflows and overflows with each combination of properties over hundreds of simulations, I made comparisons.
  - As a conclusion, I proposed how to limit buffer underflows and overflows.

#### Languages Utilized:
  - Python
