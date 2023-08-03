import logging

from dp3.common.base_module import BaseModule
from dp3.common.callback_registrar import CallbackRegistrar
from dp3.common.config import PlatformConfig


class Annotator(BaseModule):
    """
    Annotator module
    """
    def __init__(self, platform_config: PlatformConfig, module_config: dict, registrar: CallbackRegistrar):
        self.logger = logging.getLogger(__name__)
        self.logger.info("Annotator initialized")
