#!/usr/bin/env python

import argparse
import numpy as np

#
# The python bindings for generate_view are in ex-generate.cpp
# The declaration and definition of generate_view are in user.hpp and user.cpp
# The generate_view function will return a Kokkos::View and will be converted
# to a numpy array
from ex_generate import generate_view, modify_view

#
# Importing this module is necessary to call kokkos init/finalize and
# import the python bindings to Kokkos::View which generate_view will
# return
#
import kokkos


def print_data(label, name, space, data):
    # write the type info
    print(
        "{:12} : {} (ndim={}, shape={})".format(
            label, type(data).__name__, data.ndim, data.shape
        )
    )
    if space == kokkos.CudaSpace:
        return
    # print the data
    if data.ndim == 1:
        for i in range(data.shape[0]):
            print("{:8}({}) = {}".format(name, i, data[i]))
    elif data.ndim == 2:
        for i in range(data.shape[0]):
            print(
                "{:8}({}) = [{}]".format(
                    name,
                    i,
                    " ".join("{}".format(data[i, j]) for j in range(data.shape[1])),
                )
            )
    else:
        raise ValueError("only 2 dimensions are supported")


def user_bindings(args):
    # get the kokkos view
    view = generate_view(args.ndim)
    print_data("Kokkos View", "view", view.space, view)

    # modify view (verify that casting works)
    modify_view(view)
    print_data("Modify View", "view", view.space, view)

    # wrap the buffer protocal as numpy array without copying the data
    arr = np.array(view, copy=False)
    print_data("Numpy Array", "arr", view.space, arr)


def to_numpy(args):
    # get the kokkos view
    view = kokkos.array(
        "python_allocated_view",
        [args.ndim],
        dtype=kokkos.double,
        space=kokkos.HostSpace,
    )

    for i in range(view.shape[0]):
        view[i] = i * (i % 2)
    print_data("Kokkos View", "view", view.space, view)

    # wrap the buffer protocal as numpy array without copying the data
    arr = np.array(view, copy=False)
    print_data("Numpy Array", "arr", view.space, arr)


def from_numpy(args):
    arr = np.ones([args.ndim, args.ndim], dtype=np.int32)
    for i in range(args.ndim):
        arr[i, i] = 0

    print_data("Numpy Array", "arr", kokkos.HostSpace, arr)

    view = kokkos.array(arr, dtype=kokkos.int32, dynamic=True)
    print_data("Kokkos View", "view", kokkos.HostSpace, view)


if __name__ == "__main__":
    kokkos.initialize()
    parser = argparse.ArgumentParser()
    parser.add_argument("-n", "--ndim", default=10, help="X dimension", type=int)
    args, argv = parser.parse_known_args()
    user_bindings(args)
    to_numpy(args)
    from_numpy(args)
    kokkos.finalize()
