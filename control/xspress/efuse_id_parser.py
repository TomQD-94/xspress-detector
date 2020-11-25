import logging


class ExcaliburEfuseIDParser(object):

    def __init__(self):
        self._filename = None
        self._efuse_ids = {
            1: -1,
            2: -1,
            3: -1,
            4: -1,
            5: -1,
            6: -1,
            7: -1,
            8: -1
        }

    def parse_file(self, filename):
        self._filename = filename

        try:
            with open(filename) as f:
                # Read in the lines
                for line in f.readlines():
                    if line.startswith("Chip "):
                        logging.debug(line.strip("\n"))
                        chip_number = int(line[5:6])
                        id_string = line[25:].strip("\n")
                        efuse_id = int(id_string, 0)
                        self._efuse_ids[chip_number] = efuse_id
        except IOError as e:
            logging.error('Failed to parse EfuseID file: {}'.format(e))
            raise IOError('Failed to parse EfuseID file: {}'.format(e))

        logging.debug("EfuseIDs: %s", str(self._efuse_ids))

    @property
    def efuse_ids(self):
        return self._efuse_ids


def main():
    logging.getLogger().setLevel(logging.DEBUG)
    efp = ExcaliburEfuseIDParser()
    efp.parse_file('/dls_sw/i13-1/epics/excalibur/V2.7/fem1/efuseIDs')



if __name__ == '__main__':
    main()
