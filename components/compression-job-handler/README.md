# CLP Compression Job Handler

This Python module submits compression jobs to the CLP compression scheduler.

## Installation

```bash
pip3 install -r requirements.txt --target <clp-package>/lib/python3/site-packages
cp -R clp_py_utils <clp-package>/lib/python3/site-packages
```

## Usage

Below are a few ways to use this module.

### Docker compression wrapper

```bash
<clp-package>/sbin/compress <parameters>
```

### Native compression wrapper

```bash
<clp-package>/sbin/native/compress <parameters>
```

### Standalone

```bash
PYTHONPATH=<clp_home/lib/python3/site-packages> python3 -m compression_job_handler <parameters>
```
