# Gettysburg - Smithsonian Edition

A two player Hex-and-Counter game based on game published by Avalon
Hill in 1988. Uses the game board, pieces and instructions from
https://gitlab.com/wargames_tex/sgb_tex.

The following trains a network to play Gettysburg. `NOTE` that the
following first removes models and training results from
/tmp/cifre/models, so update appropriately if you don't want that data
deleted.

```bash
rm -fr /tmp/cifre/models/* && rm -fr /tmp/cifre/train/* && ./cifre_train.py --repo-dir=/tmp/cifre/models --train-dir=/tmp/cifre/train --game=gettyburg --epoch-count=20 --iteration-count=8 --game-count=100
```

Default settings for alpha-zero can be found in main.cc
Gettysburg::DefaultAlphaZeroSettings.

After training a model you can play individual games against alphazero
using that model. Assuming the best trained model is called 'iter3':

```bash
install/bin/gettysburg --player1=human --player2=alphazero --alphazero-model iter3
```

To have player2 go first add the --starting argument after --player2.

```bash
install/bin/gettysburg --player1=human --player2=alphazero --starting --alphazero-model iter3
```

You can also have a random or alphazero player that plays against a
human or each other. For example, use the following command to play a
random player against the alphazero model. Use --game-count to play
multiple games. `NOTE` it is not recommended to use a *perfect* player
with Gettysburg as the computation time for perfect play is likely not
reasonable.

```bash
install/bin/gettysburg --game-count=10 --player1=random --player2=alphazero --alphazero-model iter3
```

You can even play two alphazero models against each other.

```bash
install/bin/gettysburg --game-count=10 --player1=alphazero --alphazero-model <another model name> --player2=alphazero --alphazero-model iter3
```
