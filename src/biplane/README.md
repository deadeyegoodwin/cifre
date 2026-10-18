# Biplane

Two player game simpulating a WW1 biplane duel.

The following trains a network to play Biplane. `NOTE` that the
following first removes models and training results from
/tmp/cifre/models, so update appropriately if you don't want that data
deleted.

```bash
rm -fr /tmp/cifre/models/* && rm -fr /tmp/cifre/train/* && ./cifre_train.py --repo-dir=/tmp/cifre/models --train-dir=/tmp/cifre/train --game=biplane --epoch-count=20 --iteration-count=8 --game-count=5000
```

Default settings for alpha-zero can be found in main.cc
Biplane::DefaultAlphaZeroSettings.

After training a model you can play individual games against alphazero
using that model. Assuming the best trained model is called 'iter3':

```bash
install/bin/biplane --player1=human --player2=alphazero --alphazero-model iter3
```

You can also have a random or alphazero player that plays against a
human or each other. For example, use the following command to play a
random player against the alphazero model. Use --game-count to play
multiple games.

```bash
install/bin/biplane --game-count=10 --player1=random --player2=alphazero --alphazero-model iter3
```

You can even play two alphazero models against each other.

```bash
install/bin/biplane --game-count=10 --player1=alphazero --alphazero-model <another model name> --player2=alphazero --alphazero-model iter3
```
