## Original Asteroids Game on an Embedded System

Course: EEC 172 - Embedded Systems

#### Project Specifications:
  - Materials: Texus Instruments (TI) Simplelink CC3200 Launchpad, Adafruit 128x128 Oled Breakout Board, TI Code Composer Studio, and TI Pin Mux Tool
  - In the project directory, these files contain my code: main.c, pinmux.c, and Adafruit_OLED.c
  - The core of this lab was to create a program that accesses the web in whatever way we like and incorporate the variety of sensors and devices that we’ve used in the previous labs for input and output.
  - I decided to create my own Asteroids game that uses Amazon Web Services to store game state (eg. the high score) that can then be retrieved later.
  - My game called “-Asteroids-" is a game about dodging asteroids, which gives points, and trying to get the highest score possible.
  - A block diagram of the program is provided under Figure 2. On boot up, the program starts by connecting to a predetermined Wifi Network. Then the menu screen (shown in figure 1) with the game’s title is drawn.
  - The load save screen lets the user enter a save key from which to grab game data using a GET request.
  - The save state screen lets the user enter a save key to store the current game state under the key using POST requests.
  - The menu navigation and text entry is done using an infrared TV remote.
  - I used the multitap protocol to allow text entry of all characters in the alphabet, numbers 0 to 9, the underscore, and dash characters.
  - When playing the game, the user controls the ship by using the accelerometer of the CC3200 as a tilt-controller.
  - The player gains points for each asteroid they successfully avoid, while they lose "health points" by colliding into asteroids.
  - The game ends when the player loses all of their health points.

#### Languages Utilized:
  - C/C++

![Game UI](https://github.com/NimaBayati/Relevant-Coursework/assets/43078702/60de4f57-18c3-487e-b9a7-376324a60d27)

Figure 1: Menu Screen (left), Save State screen (center), and gameplay (right)

![System Block Diagram](https://github.com/NimaBayati/Relevant-Coursework/assets/43078702/56d4fab7-7c84-4179-8bce-5bf2a7749d3c)

Figure 2: System Block Diagram
