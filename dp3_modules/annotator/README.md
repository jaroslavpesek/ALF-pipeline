This module label the data from DP3. This is problem dependent. 
Please implement this module but the logic of labeling data should be in abstract class,
something like this:

```python
class Annotator(ABC):
    def __init__(self, config):
        self.config = config

    def label(self, data):
        raise NotImplementedError
```

