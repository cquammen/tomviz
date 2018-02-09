import tomviz.operators

import time

NUMBER_OF_CHUNKS = 10


class InvertOperator(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset):
        from tomviz import utils
        import numpy as np
        self.progress.maximum = NUMBER_OF_CHUNKS

        scalars = utils.get_scalars(dataset)
        if scalars is None:
            raise RuntimeError("No scalars found!")

        result = np.float32(scalars)
        min = np.amin(scalars)
        max = np.amax(scalars)
        step = 0
        for chunk in np.array_split(result, NUMBER_OF_CHUNKS):
            if self.canceled:
                return
            chunk[:] = max - chunk + min
            step += 1
            self.progress.value = step

            # Probably need a lock prior to updating the data
            utils.set_scalars(dataset, result)
            # Unlock

            # This notifies the main thread that new data is ready to
            # be rendered
            self.progress.data_updated()

            # Fake a long-running process
            time.sleep(1)

        utils.set_scalars(dataset, result)
