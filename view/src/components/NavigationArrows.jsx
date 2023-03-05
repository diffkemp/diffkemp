// Arrows for navigating to next/previous difference, ...
// Author: Lukas Petr

import ButtonGroup from 'react-bootstrap/ButtonGroup';
import Button from 'react-bootstrap/Button';

/**
 * Arrows for navigating to next/previous difference, ...
 * @param {Object} props
 */
export default function NavigationArrows({
  onPrevClick,
  onNextClick,
  nextText,
  prevText,
  disablePrev,
  disableNext,
  className,
}) {
  return (
    <ButtonGroup className={className}>
      <Button
        onClick={onPrevClick}
        variant="outline-primary"
        size="sm"
        disabled={disablePrev}
      >
        {`< Prev ${prevText}`}
      </Button>
      <Button
        onClick={onNextClick}
        variant="outline-primary"
        size="sm"
        disabled={disableNext}
      >
        {`Next ${nextText} >`}
      </Button>
    </ButtonGroup>
  );
}
