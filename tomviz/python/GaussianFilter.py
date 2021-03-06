def transform_scalars(dataset):
    """Apply a Gaussian filter to dataset."""
    """Gaussian Filter blurs the image and reduces the noise and details."""
    
    #----USER SPECIFIED VARIABLES-----#
    ###Sigma###    #Specify sigma of the Gaussian Function
    #---------------------------------#

    from tomviz import utils
    import numpy as np
    import scipy.ndimage

    array = utils.get_array(dataset)

    # transform the dataset
    result = scipy.ndimage.filters.gaussian_filter(array,sigma)
    
    # set the result as the new scalars.
    utils.set_array(dataset, result)
    
