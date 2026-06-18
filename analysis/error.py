ERR_OK          = 0
ERR_STORAGE     = 1001
ERR_DB          = 1002
ERR_PERF_SCRIPT = 1003
ERR_PARSE       = 1004
ERR_LLM         = 1005
ERR_CONFIG      = 1006


class AnalysisError(Exception):
    def __init__(self, code: int, msg: str):
        super().__init__(msg)
        self.code = code
        self.msg = msg
